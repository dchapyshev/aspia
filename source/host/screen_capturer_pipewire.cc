//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/screen_capturer_pipewire.h"

#include <libyuv/convert_argb.h>

#include <drm/drm_fourcc.h>

#include <spa/buffer/meta.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/region.h"
#include "host/linux/egl_dmabuf.h"
#include "host/linux/pipewire.h"
#include "host/linux/wayland_capture_source.h"
#include "host/linux/wayland_compositor_source.h"

// AlmaLinux 8 ships PipeWire 0.3.6, whose SPA headers predate these format-negotiation flags. Define
// them when the system headers are too old; on newer SPA (where they exist) the guards are no-ops.
#ifndef SPA_POD_PROP_FLAG_MANDATORY
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#ifndef SPA_POD_PROP_FLAG_DONT_FIXATE
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

namespace {

const int kBytesPerPixel = 4;

// Room for a cursor bitmap up to this size, delivered as separate metadata.
const int kMaxCursorSide = 384;

// Maximum number of damage regions requested in the video-damage metadata.
const int kMaxDamageRegions = 16;

// How long the compositor stream may stay paused before capture falls back to KMS. A normal pause
// (renegotiation) lasts a fraction of a second; a much longer one means the session is locked and the
// compositor will not resume until it unlocks, so the lock screen is only reachable below it.
const Milliseconds kPausedFallback{ 1500 };

// 32-bit packed RGB formats offered to the compositor.
const quint32 kVideoFormats[] = {
    SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
    SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA
};

//--------------------------------------------------------------------------------------------------
quint32 drmFourcc(quint32 spa_format)
{
    switch (spa_format)
    {
        case SPA_VIDEO_FORMAT_BGRA: return DRM_FORMAT_ARGB8888;
        case SPA_VIDEO_FORMAT_BGRx: return DRM_FORMAT_XRGB8888;
        case SPA_VIDEO_FORMAT_RGBA: return DRM_FORMAT_ABGR8888;
        case SPA_VIDEO_FORMAT_RGBx: return DRM_FORMAT_XBGR8888;
        default: return 0;
    }
}

//--------------------------------------------------------------------------------------------------
// Maps a compositor connector name to a stable screen id. The id only needs to be stable within the
// running host process (the client round-trips it back during the same session), so a plain FNV-1a
// hash of the connector is enough and avoids depending on monitor enumeration order.
ScreenCapturer::ScreenId connectorId(const QString& connector)
{
    quint32 hash = 2166136261u;
    for (QChar ch : connector)
        hash = (hash ^ static_cast<quint16>(ch.unicode())) * 16777619u;
    return static_cast<ScreenCapturer::ScreenId>(hash & 0x7fffffffu);
}

//--------------------------------------------------------------------------------------------------
// Builds one EnumFormat object. With |modifiers| it advertises a DMA-BUF format (the compositor
// picks a modifier); without, a plain format (delivered via MemFd/MemPtr). |dont_fixate| advertises
// a choice of modifiers; clearing it pins a single, already chosen modifier (the fixation step).
const spa_pod* buildFormat(spa_pod_builder* builder, quint32 format,
                           const QList<quint64>& modifiers, bool dont_fixate)
{
    spa_pod_frame format_frame;
    spa_pod_builder_push_object(builder, &format_frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(builder,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_Id(format),
        0);

    if (!modifiers.isEmpty())
    {
        quint32 prop_flags = SPA_POD_PROP_FLAG_MANDATORY;
        if (dont_fixate)
            prop_flags |= SPA_POD_PROP_FLAG_DONT_FIXATE;
        spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier, prop_flags);

        spa_pod_frame modifier_frame;
        spa_pod_builder_push_choice(builder, &modifier_frame, SPA_CHOICE_Enum, 0);
        spa_pod_builder_long(builder, static_cast<qint64>(modifiers.first())); // default
        for (quint64 modifier : modifiers)
            spa_pod_builder_long(builder, static_cast<qint64>(modifier));
        spa_pod_builder_pop(builder, &modifier_frame);
    }

    // spa_pod_builder_addv reads Rectangle/Fraction arguments as pointers (va_arg(spa_rectangle*)),
    // so the choice values must be passed by address, not by value.
    struct spa_rectangle size_def = SPA_RECTANGLE(1920, 1080);
    struct spa_rectangle size_min = SPA_RECTANGLE(1, 1);
    struct spa_rectangle size_max = SPA_RECTANGLE(8192, 8192);
    struct spa_fraction rate_def = SPA_FRACTION(30, 1);
    struct spa_fraction rate_min = SPA_FRACTION(0, 1);
    struct spa_fraction rate_max = SPA_FRACTION(120, 1);

    spa_pod_builder_add(builder,
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&size_def, &size_min, &size_max),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&rate_def, &rate_min, &rate_max),
        0);

    return static_cast<const spa_pod*>(spa_pod_builder_pop(builder, &format_frame));
}

//--------------------------------------------------------------------------------------------------
void onStreamStateChanged(
    void* data, pw_stream_state old_state, pw_stream_state state, const char* error)
{
    const PipeWireApi* pw = PipeWire::api();
    if (pw)
    {
        LOG(INFO) << "PipeWire stream state:" << pw->pw_stream_state_as_string(old_state) << "->"
                  << pw->pw_stream_state_as_string(state) << (error ? error : "");
    }

    if (!data)
        return;

    ScreenCapturerPipeWire* self = static_cast<ScreenCapturerPipeWire*>(data);

    // A locked session pauses the stream (the compositor stops delivering the session content); the
    // capture thread watches how long it stays paused and falls back to KMS.
    self->setStreamPaused(state == PW_STREAM_STATE_PAUSED);

    // The compositor drops the stream into the error state when it renegotiates the format in a way
    // the current connection cannot follow (a resolution change, a monitor reconfiguration, the screen
    // blanking). Re-create the stream so it negotiates the new format from scratch instead of staying
    // black. The restart itself is done on the capture thread (here we only flag it).
    if (state == PW_STREAM_STATE_ERROR)
        self->requestRestart();
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerPipeWire::ScreenCapturerPipeWire(uid_t session_uid, QObject* parent)
    : ScreenCapturer(Type::LINUX_WAYLAND, parent),
      source_(WaylandCompositorSource::create(session_uid))
{
    if (source_)
    {
        connect(source_.get(), &WaylandCompositorSource::sig_started,
                this, &ScreenCapturerPipeWire::onSourceStarted);
    }
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerPipeWire::~ScreenCapturerPipeWire()
{
    stopStream();
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerPipeWire* ScreenCapturerPipeWire::create(uid_t session_uid, QObject* parent)
{
    std::unique_ptr<ScreenCapturerPipeWire> self(new ScreenCapturerPipeWire(session_uid, parent));
    if (!self->source_)
    {
        LOG(ERROR) << "No compositor capture source available";
        return nullptr;
    }

    // Start negotiation on the next event-loop turn, not here: the caller connects to sig_started only
    // after create() returns, and the source can fail (and emit sig_started) synchronously - e.g. when
    // the compositor inhibits screencast on a locked screen. Emitting before the caller has connected
    // would lose that failure and, with it, the fallback to KMS.
    ScreenCapturerPipeWire* capturer = self.release();
    QMetaObject::invokeMethod(capturer, [capturer]() { capturer->source_->start(); },
                              Qt::QueuedConnection);
    return capturer;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerPipeWire::screenCount()
{
    if (!source_)
        return 1;

    // The Mutter ScreenCast source can record each monitor individually; the portal source exposes a
    // single user-picked stream and reports no monitors, so one screen is presented.
    const int count = source_->monitors().size();
    return count > 0 ? count : 1;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerPipeWire::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    const QList<WaylandCompositorSource::MonitorInfo> monitors =
        source_ ? source_->monitors() : QList<WaylandCompositorSource::MonitorInfo>();

    if (monitors.isEmpty())
    {
        // Single-stream source (portal): expose one screen covering the captured stream.
        Screen screen;
        screen.id = 0;
        screen.position = QPoint(0, 0);
        screen.resolution = screen_rect_.size();
        screen.dpi = QPoint(96, 96);
        screen.is_primary = true;
        screens->screens.append(screen);
        return true;
    }

    for (const WaylandCompositorSource::MonitorInfo& monitor : std::as_const(monitors))
    {
        Screen screen;
        screen.id = connectorId(monitor.connector);
        screen.title = monitor.title;
        screen.position = monitor.position;
        screen.resolution = monitor.size;
        screen.dpi = QPoint(96, 96);
        screen.is_primary = monitor.primary;
        screens->screens.append(screen);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerPipeWire::selectScreen(ScreenId screen_id)
{
    if (!source_)
        return true;

    const QList<WaylandCompositorSource::MonitorInfo> monitors = source_->monitors();
    if (monitors.isEmpty())
    {
        // Single-stream source (portal): there is nothing to switch.
        return true;
    }

    for (const WaylandCompositorSource::MonitorInfo& monitor : std::as_const(monitors))
    {
        if (connectorId(monitor.connector) != screen_id)
            continue;

        if (source_->recordedMonitor() == monitor.connector)
            return true; // Already recording this monitor.

        // Switch the recorded monitor and renegotiate the stream. Queued so it runs off the capture
        // call stack: onRestartSource() tears down the PipeWire stream and re-starts the source, which
        // records the newly requested connector.
        source_->setRequestedMonitor(monitor.connector);
        QMetaObject::invokeMethod(this, &ScreenCapturerPipeWire::onRestartSource, Qt::QueuedConnection);

        LOG(INFO) << "Selecting monitor:" << monitor.connector;
        return true;
    }

    LOG(ERROR) << "selectScreen: unknown screen id" << screen_id;
    return false;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerPipeWire::currentScreen() const
{
    if (!source_)
        return 0;

    const QString connector = source_->recordedMonitor();
    return connector.isEmpty() ? 0 : connectorId(connector);
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerPipeWire::captureFrame(Error* error)
{
    // The stream errored on a renegotiation (resolution change, monitor reconfiguration, screen
    // blank). The compositor may have produced a brand-new node, so reconnecting to the old one loops
    // forever - re-negotiate the whole capture source instead. Queued so it runs off the capture call
    // stack (it tears down and rebuilds the stream).
    if (restart_requested_.exchange(false))
    {
        LOG(INFO) << "PipeWire stream error; re-negotiating the capture source";
        QMetaObject::invokeMethod(this, &ScreenCapturerPipeWire::onRestartSource, Qt::QueuedConnection);
        *error = Error::TEMPORARY;
        return nullptr;
    }

    // A prolonged paused state means the compositor stopped delivering frames without an error - most
    // often the session locked (the compositor pauses screencast while locked; the lock screen itself is
    // only reachable below the compositor). Fall back to KMS so the lock screen is captured instead of a
    // frozen frame. Routed through the owner (parent) queued, so the fallback - which deletes this
    // capturer - does not run on captureFrame()'s stack.
    if (stream_paused_.load(std::memory_order_relaxed))
    {
        if (!paused_since_.isValid())
            paused_since_.start();

        if (!fallback_requested_ && paused_since_.hasExpired(kPausedFallback.count()))
        {
            fallback_requested_ = true;
            LOG(INFO) << "Compositor stream paused for" << kPausedFallback.count()
                      << "ms (session likely locked); falling back to KMS";
            QMetaObject::invokeMethod(parent(), [this]() { emit sig_started(false); },
                                      Qt::QueuedConnection);
        }
    }
    else
    {
        paused_since_.invalidate();
        fallback_requested_ = false;
    }

    QMutexLocker locker(&frame_mutex_);

    if (has_new_frame_)
    {
        Frame* frame = queue_.currentFrame();
        if (!frame)
        {
            *error = Error::TEMPORARY;
            return nullptr;
        }

        frame->setCapturerType(static_cast<quint32>(type()));
        screen_rect_ = QRect(QPoint(0, 0), frame->size());

        // Report the damaged regions accumulated since the last capture, or the whole frame when no
        // damage information was available.
        if (full_damage_)
            *frame->updatedRegion() = screen_rect_;
        else
            *frame->updatedRegion() = accumulated_damage_;

        accumulated_damage_.clear();
        full_damage_ = false;

        // Advance the queue: the producer keeps writing the new current frame while the consumer
        // encodes this one (now the previous frame), so the two never touch the same buffer.
        queue_.moveToNextFrame();
        has_new_frame_ = false;

        *error = Error::SUCCEEDED;
        return frame;
    }

    // No new frame: return the last one with an empty region so it is not re-encoded.
    Frame* frame = queue_.previousFrame();
    if (!frame)
    {
        *error = Error::TEMPORARY;
        return nullptr;
    }

    frame->setCapturerType(static_cast<quint32>(type()));
    frame->updatedRegion()->clear();
    *error = Error::SUCCEEDED;
    return frame;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerPipeWire::captureCursor()
{
    QMutexLocker locker(&frame_mutex_);

    // Returns a non-null cursor only when its shape changed, so it is sent once per change.
    if (!cursor_changed_ || cursor_data_.isEmpty())
        return nullptr;

    cursor_changed_ = false;

    QByteArray image = cursor_data_;
    current_cursor_ = std::make_unique<MouseCursor>(std::move(image), cursor_size_, cursor_hotspot_);
    return current_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerPipeWire::cursorPosition()
{
    QMutexLocker locker(&frame_mutex_);
    return cursor_position_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerPipeWire::desktopRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerPipeWire::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::reset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerPipeWire::init()
{
    if (!source_ || !source_->isStarted())
    {
        LOG(ERROR) << "Wayland portal is not started";
        return false;
    }

    screen_rect_ = QRect(QPoint(0, 0), source_->stream().size);
    return startStream();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerPipeWire::startStream()
{
    if (!source_)
        return false;

    if (!PipeWire::ensureLoaded())
        return false;

    api_ = PipeWire::api();
    if (!api_)
        return false;

    // Off-screen EGL/GBM importer for DMA-BUF frames. If it fails to initialize, only MemFd/MemPtr
    // formats are offered and capture still works. Created once and reused across stream restarts:
    // re-initializing EGL/GBM on every monitor switch churns the driver's EGL display/context and
    // crashes Mesa (libgallium GPF) after a few cycles, so it must outlive individual streams.
    if (!egl_dmabuf_)
        egl_dmabuf_ = std::make_unique<EglDmaBuf>();

    api_->pw_init(nullptr, nullptr);

    thread_loop_ = api_->pw_thread_loop_new("aspia-pipewire", nullptr);
    if (!thread_loop_)
    {
        LOG(ERROR) << "pw_thread_loop_new failed";
        return false;
    }

    context_ = api_->pw_context_new(api_->pw_thread_loop_get_loop(thread_loop_), nullptr, 0);
    if (!context_)
    {
        LOG(ERROR) << "pw_context_new failed";
        return false;
    }

    if (api_->pw_thread_loop_start(thread_loop_) < 0)
    {
        LOG(ERROR) << "pw_thread_loop_start failed";
        return false;
    }

    api_->pw_thread_loop_lock(thread_loop_);

    const int source_fd = source_->pipeWireFd();
    if (source_fd >= 0)
    {
        // The portal handed us a descriptor. pw_context_connect_fd() takes ownership and closes it on
        // disconnect, so pass a private duplicate and leave the source's descriptor untouched.
        int own_fd = ::dup(source_fd);
        if (own_fd < 0)
        {
            LOG(ERROR) << "Unable to duplicate the PipeWire descriptor";
            api_->pw_thread_loop_unlock(thread_loop_);
            return false;
        }

        core_ = api_->pw_context_connect_fd(context_, own_fd, nullptr, 0);
        if (!core_)
        {
            LOG(ERROR) << "pw_context_connect_fd failed";
            ::close(own_fd);
            api_->pw_thread_loop_unlock(thread_loop_);
            return false;
        }
    }
    else
    {
        // The stream lives on the session's own PipeWire daemon (Mutter ScreenCast). The root host has
        // no XDG_RUNTIME_DIR of its own, and older PipeWire (e.g. 0.3.6 on RHEL 8) builds the socket path
        // from XDG_RUNTIME_DIR plus the remote name rather than accepting a full path - so point the
        // runtime dir at the session user's and pass the plain socket name. root can open it (the socket
        // is not uid-restricted). pw_context_connect takes ownership of the properties.
        const uid_t uid = source_->pipeWireUid();
        pw_properties* properties = nullptr;
        if (uid != static_cast<uid_t>(-1))
        {
            const QByteArray runtime_dir = QString("/run/user/%1").arg(uid).toLocal8Bit();
            setenv("XDG_RUNTIME_DIR", runtime_dir.constData(), 1);
            properties = api_->pw_properties_new(PW_KEY_REMOTE_NAME, "pipewire-0", nullptr);
        }

        core_ = api_->pw_context_connect(context_, properties, 0);
        if (!core_)
        {
            LOG(ERROR) << "pw_context_connect failed";
            api_->pw_thread_loop_unlock(thread_loop_);
            return false;
        }
    }

    stream_ = api_->pw_stream_new(core_, "aspia-capture", api_->pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        nullptr));
    if (!stream_)
    {
        LOG(ERROR) << "pw_stream_new failed";
        api_->pw_thread_loop_unlock(thread_loop_);
        return false;
    }

    stream_listener_ = std::make_unique<spa_hook>();
    spa_zero(*stream_listener_);

    static const pw_stream_events kStreamEvents =
    {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = &onStreamStateChanged,
        .param_changed = &ScreenCapturerPipeWire::onParamChanged,
        .process = &ScreenCapturerPipeWire::onProcess,
    };

    api_->pw_stream_add_listener(stream_, stream_listener_.get(), &kStreamEvents, this);

    quint8 pod_buffer[16384];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));

    const spa_pod* params[16];
    // Skip DMA-BUF formats if a previous stream already proved the import path unusable on this GPU,
    // so a switch does not repeat the failed-import then renegotiate-to-shm cycle on every stream.
    const int n_params = buildFormatParams(&builder, params, !dmabuf_disabled_);

    int ret = api_->pw_stream_connect(stream_, PW_DIRECTION_INPUT, source_->stream().node_id,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, n_params);

    api_->pw_thread_loop_unlock(thread_loop_);

    if (ret < 0)
    {
        LOG(ERROR) << "pw_stream_connect failed:" << ret;
        return false;
    }

    LOG(INFO) << "PipeWire stream started (node:" << source_->stream().node_id << ")";
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::stopStream()
{
    if (!thread_loop_ || !api_)
        return;

    api_->pw_thread_loop_stop(thread_loop_);

    if (stream_)
    {
        api_->pw_stream_destroy(stream_);
        stream_ = nullptr;
    }

    if (core_)
    {
        api_->pw_core_disconnect(core_);
        core_ = nullptr;
    }

    if (context_)
    {
        api_->pw_context_destroy(context_);
        context_ = nullptr;
    }

    api_->pw_thread_loop_destroy(thread_loop_);
    thread_loop_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::requestRestart()
{
    restart_requested_.store(true);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::setStreamPaused(bool paused)
{
    stream_paused_.store(paused, std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
WaylandCompositorSource* ScreenCapturerPipeWire::compositorSource() const
{
    return source_.get();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::onSourceStarted(bool success)
{
    if (success && init())
    {
        emit sig_started(true);
    }
    else
    {
        LOG(ERROR) << "Capture source failed to start";
        emit sig_started(false);
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::onRestartSource()
{
    stopStream();
    if (source_)
        source_->start();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerPipeWire::buildFormatParams(
    spa_pod_builder* builder, const spa_pod** params, bool allow_dmabuf)
{
    int n = 0;

    // Offer DMA-BUF formats (with the modifiers the GPU supports plus the implicit one) first, then
    // the same formats without modifiers as a MemFd/MemPtr fallback.
    if (allow_dmabuf && egl_dmabuf_ && egl_dmabuf_->isInitialized())
    {
        for (quint32 format : kVideoFormats)
        {
            QList<quint64> modifiers = egl_dmabuf_->queryModifiers(drmFourcc(format));
            modifiers.append(DRM_FORMAT_MOD_INVALID);
            params[n++] = buildFormat(builder, format, modifiers, true);
        }
    }

    for (quint32 format : kVideoFormats)
        params[n++] = buildFormat(builder, format, {}, false);

    return n;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::disableDmaBufAndRenegotiate()
{
    LOG(WARNING) << "DMA-BUF import failed; renegotiating without DMA-BUF (shared memory)";
    dmabuf_disabled_ = true;

    quint8 pod_buffer[16384];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));

    const spa_pod* params[16];
    const int n_params = buildFormatParams(&builder, params, false);

    api_->pw_stream_update_params(stream_, params, n_params);
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerPipeWire::onParamChanged(void* data, quint32 id, const spa_pod* param)
{
    static_cast<ScreenCapturerPipeWire*>(data)->handleParamChanged(id, param);
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerPipeWire::onProcess(void* data)
{
    static_cast<ScreenCapturerPipeWire*>(data)->handleProcess();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::handleParamChanged(quint32 id, const spa_pod* param)
{
    if (!param || id != SPA_PARAM_Format)
        return;

    quint32 media_type = 0;
    quint32 media_subtype = 0;
    if (spa_format_parse(param, &media_type, &media_subtype) < 0)
        return;

    if (media_type != SPA_MEDIA_TYPE_video || media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    spa_video_info_raw info = {};
    if (spa_format_video_raw_parse(param, &info) < 0)
    {
        LOG(ERROR) << "Unable to parse the video format";
        return;
    }

    const int width = static_cast<int>(info.size.width);
    const int height = static_cast<int>(info.size.height);
    const int stride = width * kBytesPerPixel;

    // Only the PipeWire thread touches these (this callback and process() share the loop).
    format_size_ = QSize(width, height);
    swap_red_blue_ = (info.format == SPA_VIDEO_FORMAT_RGBx || info.format == SPA_VIDEO_FORMAT_RGBA);
    format_fourcc_ = drmFourcc(info.format);

    // A modifier is only present (DMA-BUF) when the format actually carried the modifier property;
    // otherwise the frames come as MemFd/MemPtr.
    const spa_pod_prop* modifier_prop = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    format_modifier_ = modifier_prop ? static_cast<quint64>(info.modifier) : DRM_FORMAT_MOD_INVALID;

    LOG(INFO) << "PipeWire format negotiated:" << width << "x" << height << "format:" << info.format
              << "modifier:" << (format_modifier_ != DRM_FORMAT_MOD_INVALID);

    // The compositor offered a modifier but asked us to fixate it: re-send the format with the chosen
    // modifier pinned and wait for the next param_changed before requesting buffers.
    if (modifier_prop && (modifier_prop->flags & SPA_POD_PROP_FLAG_DONT_FIXATE))
    {
        quint8 fixate_buffer[1024];
        spa_pod_builder fixate_builder = SPA_POD_BUILDER_INIT(fixate_buffer, sizeof(fixate_buffer));
        const spa_pod* fixate_params[1];
        fixate_params[0] = buildFormat(&fixate_builder, info.format, { format_modifier_ }, false);
        api_->pw_stream_update_params(stream_, fixate_params, 1);
        return;
    }

    // Allow shared-memory and DMA-BUF buffers, and request the metadata we use: the cursor (so it is
    // delivered separately), a header (corrupted-buffer detection), a video crop (valid region) and
    // video damage (changed regions).
    quint8 pod_buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));

    const spa_pod* params[5];
    params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 2, 16),
        SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * height),
        SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
        SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
        SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(
            (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_DmaBuf))));
    params[1] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Cursor),
        SPA_PARAM_META_size, SPA_POD_Int(static_cast<int>(
            sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) +
            kMaxCursorSide * kMaxCursorSide * kBytesPerPixel))));
    params[2] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
        SPA_PARAM_META_size, SPA_POD_Int(static_cast<int>(sizeof(struct spa_meta_header)))));
    params[3] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop),
        SPA_PARAM_META_size, SPA_POD_Int(static_cast<int>(sizeof(struct spa_meta_region)))));
    params[4] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
        SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(
            static_cast<int>(sizeof(struct spa_meta_region) * kMaxDamageRegions),
            static_cast<int>(sizeof(struct spa_meta_region)),
            static_cast<int>(sizeof(struct spa_meta_region) * kMaxDamageRegions))));

    api_->pw_stream_update_params(stream_, params, 5);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerPipeWire::handleProcess()
{
    // Drain the queue and keep only the most recent buffer; older ones are requeued immediately so
    // the capture never lags behind the compositor (as WebRTC does).
    pw_buffer* buffer = nullptr;
    for (pw_buffer* next = api_->pw_stream_dequeue_buffer(stream_); next;
         next = api_->pw_stream_dequeue_buffer(stream_))
    {
        if (buffer)
            api_->pw_stream_queue_buffer(stream_, buffer);
        buffer = next;
    }

    if (!buffer)
        return;

    spa_buffer* spa_buffer = buffer->buffer;
    if (!spa_buffer || spa_buffer->n_datas == 0)
    {
        api_->pw_stream_queue_buffer(stream_, buffer);
        return;
    }

    spa_data* data = &spa_buffer->datas[0];

    // Skip buffers the compositor marked as corrupted.
    const spa_meta_header* header = static_cast<const spa_meta_header*>(
        spa_buffer_find_meta_data(spa_buffer, SPA_META_Header, sizeof(spa_meta_header)));
    const bool corrupted =
        (header && (header->flags & SPA_META_HEADER_FLAG_CORRUPTED)) ||
        (data->chunk && (data->chunk->flags & SPA_CHUNK_FLAG_CORRUPTED));

    // The buffer may be larger than the actual content; the video-crop metadata gives the valid
    // region inside it.
    QSize frame_size = format_size_;
    int crop_x = 0;
    int crop_y = 0;
    const spa_meta_region* crop = static_cast<const spa_meta_region*>(
        spa_buffer_find_meta_data(spa_buffer, SPA_META_VideoCrop, sizeof(spa_meta_region)));
    if (crop && spa_meta_region_is_valid(crop))
    {
        crop_x = crop->region.position.x;
        crop_y = crop->region.position.y;
        frame_size = QSize(crop->region.size.width, crop->region.size.height);
    }

    // A DMA-BUF carries no CPU pointer; shared-memory buffers must have non-empty data.
    const bool has_data = (data->type == SPA_DATA_DmaBuf) ||
                          (data->data && data->chunk && data->chunk->size > 0);

    bool dmabuf_import_failed = false;

    if (!corrupted && has_data && !frame_size.isEmpty())
    {
        QMutexLocker locker(&frame_mutex_);

        // Reuse the current queue frame; reallocate only when the size changes. The producer always
        // writes the current frame and never advances the queue (the consumer does), so it cannot
        // overwrite the frame being encoded.
        if (!queue_.currentFrame() || queue_.currentFrame()->size() != frame_size)
        {
            queue_.replaceCurrentFrame(FrameAligned::create(frame_size, 32));
            full_damage_ = true; // a fresh buffer has no valid diff against the previous one
        }

        Frame* frame = queue_.currentFrame();
        if (frame)
        {
            const int dst_stride = frame->stride();
            quint8* dst = frame->frameData();
            bool wrote = false;

            if (data->type == SPA_DATA_DmaBuf)
            {
                // GPU buffer: import via EGL and read back the valid region as BGRA. The DRM fourcc
                // already encodes the channel order, so no red/blue swap is needed here.
                std::array<EglDmaBuf::Plane, 4> planes;
                const int plane_count =
                    std::min<int>(spa_buffer->n_datas, static_cast<int>(planes.size()));
                bool planes_valid = true;
                for (int i = 0; i < plane_count; ++i)
                {
                    // Every plane must carry chunk metadata; the has_data check above only validated the
                    // first plane, so guard the rest before dereferencing.
                    const auto* chunk = spa_buffer->datas[i].chunk;
                    if (!chunk)
                    {
                        planes_valid = false;
                        break;
                    }

                    planes[i].fd = static_cast<int>(spa_buffer->datas[i].fd);
                    planes[i].offset = chunk->offset;
                    planes[i].stride = static_cast<quint32>(chunk->stride);
                }

                const QRect read_rect(crop_x, crop_y, frame_size.width(), frame_size.height());
                if (planes_valid && egl_dmabuf_ && egl_dmabuf_->imageFromDmaBuf(
                        format_size_, format_fourcc_, planes.data(), plane_count, format_modifier_,
                        read_rect, dst, dst_stride))
                {
                    wrote = true;
                }
                else
                {
                    dmabuf_import_failed = true;
                }
            }
            else
            {
                // Shared memory (MemFd/MemPtr): copy the valid region with libyuv. The chunk offset
                // and crop position locate the first pixel. BGRx/BGRA is a straight copy; RGBx/RGBA
                // needs the red and blue channels swapped.
                int src_stride = static_cast<int>(data->chunk->stride);
                if (src_stride <= 0)
                    src_stride = format_size_.width() * kBytesPerPixel;

                // The crop region and stride are producer-supplied. Validate that the source rectangle
                // stays within both the negotiated format and the mapped buffer before indexing, or
                // libyuv would read out of bounds. Compute in 64-bit and drop the frame on any mismatch.
                const qint64 crop_w = frame_size.width();
                const qint64 crop_h = frame_size.height();
                const qint64 end_offset = static_cast<qint64>(data->chunk->offset) +
                    (static_cast<qint64>(crop_y) + crop_h - 1) * src_stride +
                    (static_cast<qint64>(crop_x) + crop_w) * kBytesPerPixel;

                if (crop_x < 0 || crop_y < 0 ||
                    crop_x + crop_w > format_size_.width() ||
                    crop_y + crop_h > format_size_.height() ||
                    end_offset > static_cast<qint64>(data->maxsize))
                {
                    LOG(ERROR) << "Invalid video crop region, dropping frame";
                }
                else
                {
                    const quint8* src = static_cast<const quint8*>(data->data) +
                        data->chunk->offset + crop_y * src_stride + crop_x * kBytesPerPixel;

                    if (swap_red_blue_)
                        libyuv::ABGRToARGB(src, src_stride, dst, dst_stride,
                                           frame_size.width(), frame_size.height());
                    else
                        libyuv::ARGBCopy(src, src_stride, dst, dst_stride,
                                         frame_size.width(), frame_size.height());

                    wrote = true;
                }
            }

            if (wrote)
            {
                has_new_frame_ = true;

                // Accumulate damage so captureFrame() reports only the changed regions. Without
                // damage metadata, fall back to a full frame.
                const QRect frame_rect(QPoint(0, 0), frame_size);
                spa_meta* damage_meta = spa_buffer_find_meta(spa_buffer, SPA_META_VideoDamage);
                if (damage_meta)
                {
                    spa_meta_region* region;
                    spa_meta_for_each(region, damage_meta)
                    {
                        if (!spa_meta_region_is_valid(region))
                            break;

                        // Damage is in full-buffer coordinates; the frame starts at the crop origin.
                        const QRect damaged = QRect(region->region.position.x,
                                                    region->region.position.y,
                                                    region->region.size.width,
                                                    region->region.size.height)
                                                  .translated(-crop_x, -crop_y).intersected(frame_rect);
                        if (!damaged.isEmpty())
                            accumulated_damage_ += damaged;
                    }
                }
                else
                {
                    full_damage_ = true;
                }
            }
        }
    }

    // If the GPU buffer could not be imported, drop DMA-BUF and renegotiate to shared memory so
    // capture keeps working (as WebRTC does).
    if (dmabuf_import_failed && !dmabuf_disabled_)
        disableDmaBufAndRenegotiate();

    // The cursor arrives as separate metadata: a position on every buffer and a bitmap only when the
    // shape changes.
    spa_meta* cursor_area = spa_buffer_find_meta(spa_buffer, SPA_META_Cursor);
    spa_meta_cursor* cursor_meta =
        (cursor_area && cursor_area->size >= sizeof(spa_meta_cursor)) ?
            static_cast<spa_meta_cursor*>(cursor_area->data) : nullptr;
    if (cursor_meta && spa_meta_cursor_is_valid(cursor_meta))
    {
        QMutexLocker locker(&frame_mutex_);

        cursor_position_ = QPoint(cursor_meta->position.x, cursor_meta->position.y);

        const quint64 meta_size = cursor_area->size;

        if (cursor_meta->bitmap_offset &&
            static_cast<quint64>(cursor_meta->bitmap_offset) + sizeof(spa_meta_bitmap) <= meta_size)
        {
            const spa_meta_bitmap* bitmap = reinterpret_cast<const spa_meta_bitmap*>(
                reinterpret_cast<const quint8*>(cursor_meta) + cursor_meta->bitmap_offset);

            const quint32 cursor_width = bitmap->size.width;
            const quint32 cursor_height = bitmap->size.height;
            const qint32 src_stride = bitmap->stride;

            // Absolute offset and byte extent of the pixel data inside the meta area.
            const quint64 pixels_offset =
                static_cast<quint64>(cursor_meta->bitmap_offset) + bitmap->offset;
            const quint64 pixels_extent =
                static_cast<quint64>(cursor_height) * (src_stride > 0 ? static_cast<quint64>(src_stride) : 0);

            if (bitmap->format && bitmap->offset &&
                cursor_width > 0 && cursor_width <= static_cast<quint32>(kMaxCursorSide) &&
                cursor_height > 0 && cursor_height <= static_cast<quint32>(kMaxCursorSide) &&
                src_stride >= static_cast<qint32>(cursor_width * kBytesPerPixel) &&
                pixels_offset + pixels_extent <= meta_size)
            {
                const int width = static_cast<int>(cursor_width);
                const int height = static_cast<int>(cursor_height);
                const quint8* src_bitmap = reinterpret_cast<const quint8*>(bitmap) + bitmap->offset;

                const bool swap = (bitmap->format == SPA_VIDEO_FORMAT_RGBx ||
                                   bitmap->format == SPA_VIDEO_FORMAT_RGBA);

                cursor_data_.resize(width * height * kBytesPerPixel);
                quint8* dst = reinterpret_cast<quint8*>(cursor_data_.data());
                const int dst_stride = width * kBytesPerPixel;

                if (swap)
                    libyuv::ABGRToARGB(src_bitmap, src_stride, dst, dst_stride, width, height);
                else
                    libyuv::ARGBCopy(src_bitmap, src_stride, dst, dst_stride, width, height);

                cursor_size_ = QSize(width, height);
                cursor_hotspot_ = QPoint(cursor_meta->hotspot.x, cursor_meta->hotspot.y);
                cursor_changed_ = true;
            }
        }
    }

    api_->pw_stream_queue_buffer(stream_, buffer);
}

