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

#include "base/desktop/screen_capturer_kwin.h"

#include <libyuv/convert_argb.h>
#include <libyuv/planar_functions.h>
#include <libyuv/scale_argb.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QVariant>

#include <drm/drm_fourcc.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/desktop/differ.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/linux/egl_dmabuf.h"
#include "base/linux/libdrm.h"
#include "base/linux/session_dbus.h"

namespace {

const char kService[] = "org.kde.KWin.ScreenShot2";
const char kPath[] = "/org/kde/KWin/ScreenShot2";
const char kInterface[] = "org.kde.KWin.ScreenShot2";

const int kAlignment = 32;
// QImage::Format_RGBA8888 - bytes R,G,B,A in memory; everything else KWin returns (RGB32/ARGB32/
// ARGB32_Premultiplied) is B,G,R,A, which matches the frame's packed BGRA directly.
const quint32 kFormatRgba8888 = 17;

const int kMaxCards = 8;
// A hardware cursor framebuffer is small; this tells it apart from the screen-sized primary plane.
const int kMaxCursorSize = 256;

//--------------------------------------------------------------------------------------------------
// Maps a compositor output name to a stable screen id. The id only needs to be stable within the
// running host process (the client round-trips it back during the same session), so a plain FNV-1a
// hash of the name is enough and avoids depending on output enumeration order.
ScreenCapturer::ScreenId outputId(const QString& name)
{
    quint32 hash = 2166136261u;
    for (QChar ch : name)
        hash = (hash ^ static_cast<quint16>(ch.unicode())) * 16777619u;
    return static_cast<ScreenCapturer::ScreenId>(hash & 0x7fffffffu);
}

//--------------------------------------------------------------------------------------------------
// Closes the unique GEM handles opened by drmModeGetFB2() for |fb|.
void closeFbHandles(int drm_fd, drmModeFB2* fb)
{
    for (int i = 0; i < 4; ++i)
    {
        if (!fb->handles[i])
            continue;

        bool duplicate = false;
        for (int j = 0; j < i; ++j)
        {
            if (fb->handles[j] == fb->handles[i])
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            LibDrm::closeBufferHandle(drm_fd, fb->handles[i]);
    }
}

//--------------------------------------------------------------------------------------------------
// Imports framebuffer |fb| into |dst| (|dst_stride| bytes per row) as packed BGRA via EGL/GBM. The
// GEM handles opened by drmModeGetFB2() are released here; the caller still owns and frees |fb|.
bool importFb(EglDmaBuf* egl_dmabuf, int drm_fd, drmModeFB2* fb, quint8* dst, int dst_stride)
{
    const QSize size(static_cast<int>(fb->width), static_cast<int>(fb->height));
    const quint32 fourcc = fb->pixel_format;
    const quint64 modifier =
        (fb->flags & DRM_MODE_FB_MODIFIERS) ? fb->modifier : DRM_FORMAT_MOD_INVALID;

    std::array<EglDmaBuf::Plane, 4> planes;
    std::array<int, 4> prime_fds = { -1, -1, -1, -1 };
    int plane_count = 0;

    for (int i = 0; i < 4; ++i)
    {
        if (!fb->handles[i])
            continue;

        int prime_fd = -1;
        if (LibDrm::primeHandleToFD(drm_fd, fb->handles[i], DRM_CLOEXEC, &prime_fd) != 0 ||
            prime_fd < 0)
        {
            LOG(ERROR) << "drmPrimeHandleToFD failed for plane" << i;
            break;
        }

        prime_fds[plane_count] = prime_fd;
        planes[plane_count].fd = prime_fd;
        planes[plane_count].offset = fb->offsets[i];
        planes[plane_count].stride = fb->pitches[i];
        ++plane_count;
    }

    bool ok = false;
    if (plane_count > 0)
    {
        ok = egl_dmabuf->imageFromDmaBuf(size, fourcc, planes.data(), plane_count, modifier,
                                         QRect(QPoint(0, 0), size), dst, dst_stride);
    }

    for (int i = 0; i < plane_count; ++i)
    {
        if (prime_fds[i] >= 0)
            ::close(prime_fds[i]);
    }

    closeFbHandles(drm_fd, fb);
    return ok;
}

//--------------------------------------------------------------------------------------------------
// Reads the small linear cursor framebuffer |fb| into |dst| (packed BGRA, |dst_stride| bytes per row).
// Tries a dumb CPU mapping first - it works on virtual GPUs that cannot export the cursor as a dmabuf
// (vmwgfx) - and falls back to the EGL/dmabuf import for real GPUs. The cursor is ARGB8888, which is
// packed BGRA in memory, so it copies straight across.
bool readCursorFb(EglDmaBuf* egl_dmabuf, int drm_fd, drmModeFB2* fb, quint8* dst, int dst_stride,
                  const QSize& size)
{
    uint64_t offset = 0;
    if (fb->handles[0] && LibDrm::mapDumbBuffer(drm_fd, fb->handles[0], &offset) == 0)
    {
        const size_t map_size = static_cast<size_t>(fb->pitches[0]) * fb->height;
        void* map = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, drm_fd, static_cast<off_t>(offset));
        if (map != MAP_FAILED)
        {
            libyuv::ARGBCopy(static_cast<const quint8*>(map), static_cast<int>(fb->pitches[0]),
                             dst, dst_stride, size.width(), size.height());
            munmap(map, map_size);
            closeFbHandles(drm_fd, fb);
            return true;
        }
    }

    return importFb(egl_dmabuf, drm_fd, fb, dst, dst_stride);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerKwin::ScreenCapturerKwin(uid_t session_uid, QObject* parent)
    : ScreenCapturer(Type::LINUX_KWIN, parent),
      session_uid_(session_uid),
      connection_name_(QString("aspia-kwin-%1").arg(session_uid)),
      bus_(SessionDBus::connectAsUser(session_uid, connection_name_))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerKwin::~ScreenCapturerKwin()
{
    if (drm_fd_ >= 0)
        ::close(drm_fd_);

    if (bus_.isConnected())
        QDBusConnection::disconnectFromBus(connection_name_);
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerKwin::isAvailable(uid_t session_uid)
{
    const QString name = QString("aspia-kwin-probe-%1").arg(session_uid);
    QDBusConnection bus = SessionDBus::connectAsUser(session_uid, name);

    bool available = false;
    if (bus.isConnected() && bus.interface())
        available = bus.interface()->isServiceRegistered(kService).value();

    if (bus.isConnected())
        QDBusConnection::disconnectFromBus(name);
    return available;
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerKwin* ScreenCapturerKwin::create(uid_t session_uid, QObject* parent)
{
    std::unique_ptr<ScreenCapturerKwin> self(new ScreenCapturerKwin(session_uid, parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKwin::init()
{
    if (!bus_.isConnected())
    {
        LOG(ERROR) << "No session bus connection";
        return false;
    }

    screenshot_ = new QDBusInterface(kService, kPath, kInterface, bus_, this);
    if (!screenshot_->isValid())
    {
        LOG(ERROR) << "org.kde.KWin.ScreenShot2 is not available";
        return false;
    }

    // Prime the geometry and the first frame so screenList() is valid before the capture loop.
    Error error = Error::TEMPORARY;
    if (!captureFrame(&error) || error != Error::SUCCEEDED)
    {
        LOG(ERROR) << "Initial KWin capture failed";
        return false;
    }

    initCursorCapture();

    LOG(INFO) << "KWin ScreenShot2 capturer initialized, workspace:" << screen_rect_.size();
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerKwin::initCursorCapture()
{
    if (!LibDrm::ensureLoaded())
        return;

    // Find the KMS card that exposes CRTCs (the render node has none).
    for (int i = 0; i < kMaxCards; ++i)
    {
        const QByteArray path = QByteArray("/dev/dri/card") + QByteArray::number(i);
        int fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            continue;

        drmModeRes* resources = LibDrm::modeGetResources(fd);
        if (resources && resources->count_crtcs > 0)
        {
            LibDrm::modeFreeResources(resources);
            drm_fd_ = fd;
            // The compositor owns DRM master; never take it (would block its modesetting). Reading the
            // cursor plane with CAP_SYS_ADMIN does not need master.
            if (LibDrm::dropMaster(fd) != 0)
                LOG(INFO) << "drmDropMaster: not master (expected)";
            break;
        }

        if (resources)
            LibDrm::modeFreeResources(resources);
        ::close(fd);
    }

    if (drm_fd_ < 0)
    {
        LOG(INFO) << "No DRM device for cursor capture; cursor will not be shown";
        return;
    }

    if (LibDrm::setClientCap(drm_fd_, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
        LOG(INFO) << "DRM universal planes unavailable; hardware cursor will not be captured";

    egl_dmabuf_ = std::make_unique<EglDmaBuf>();
    if (!egl_dmabuf_->isInitialized())
    {
        LOG(INFO) << "EGL/GBM import not available; cursor will not be shown";
        egl_dmabuf_.reset();
        ::close(drm_fd_);
        drm_fd_ = -1;
    }
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKwin::capture()
{
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0)
    {
        PLOG(ERROR) << "pipe2 failed";
        return false;
    }

    QVariantMap options;
    options.insert("include-cursor", false);

    // KWin replies with the metadata and streams the pixels into the pipe from a worker thread, so the
    // blocking call returns before the read below drains the pipe.
    QDBusMessage reply;
    {
        QDBusUnixFileDescriptor write_fd(fds[1]);
        ::close(fds[1]);
        if (selected_output_.isEmpty())
        {
            reply = screenshot_->call(QDBus::Block, "CaptureWorkspace",
                                      QVariant::fromValue(options), QVariant::fromValue(write_fd));
        }
        else
        {
            reply = screenshot_->call(QDBus::Block, "CaptureScreen", selected_output_,
                                      QVariant::fromValue(options), QVariant::fromValue(write_fd));
        }
    }

    if (reply.type() != QDBusMessage::ReplyMessage)
    {
        LOG(ERROR) << "CaptureWorkspace failed:" << reply.errorMessage();
        ::close(fds[0]);
        return false;
    }

    const QVariantMap results = qdbus_cast<QVariantMap>(reply.arguments().value(0));
    const int width = results.value("width").toInt();
    const int height = results.value("height").toInt();
    const int stride = results.value("stride").toInt();
    const quint32 format = results.value("format").toUInt();

    if (width <= 0 || height <= 0 || stride < width * 4)
    {
        LOG(ERROR) << "Invalid capture metadata: size" << width << height << "stride" << stride;
        ::close(fds[0]);
        return false;
    }

    const QSize size(width, height);
    const qint64 total = static_cast<qint64>(height) * stride;

    read_buffer_.resize(static_cast<qsizetype>(total));
    qint64 received = 0;
    while (received < total)
    {
        const ssize_t count = ::read(fds[0], read_buffer_.data() + received, total - received);
        if (count > 0)
            received += count;
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }
    ::close(fds[0]);

    if (received != total)
    {
        LOG(ERROR) << "Short read from KWin pipe:" << received << "/" << total;
        return false;
    }

    // Capture alternately into the two queue frames so the previous one can be diffed against.
    if (!queue_.currentFrame() || queue_.currentFrame()->size() != size)
    {
        std::unique_ptr<Frame> frame = FrameAligned::create(size, kAlignment);
        if (!frame)
            return false;
        frame->setCapturerType(static_cast<quint32>(type()));
        queue_.replaceCurrentFrame(std::move(frame));
    }

    Frame* current = queue_.currentFrame();
    const quint8* src = reinterpret_cast<const quint8*>(read_buffer_.constData());

    if (format == kFormatRgba8888)
        libyuv::ABGRToARGB(src, stride, current->frameData(), current->stride(), width, height);
    else
        libyuv::ARGBCopy(src, stride, current->frameData(), current->stride(), width, height);

    // Input-mapping geometry: the compositor maps the absolute pointer over the whole logical workspace,
    // so report the workspace bounding box and the captured output's offset within it (zero when the
    // whole workspace is captured). Falls back to the captured frame alone when the layout is unknown.
    QRect desktop;
    for (const WaylandOutputLayout::Output& output : std::as_const(outputs_))
        desktop = desktop.united(output.logical);

    QPoint offset(0, 0);
    if (!selected_output_.isEmpty())
    {
        for (const WaylandOutputLayout::Output& output : std::as_const(outputs_))
        {
            if (output.name == selected_output_)
            {
                offset = output.logical.topLeft() - desktop.topLeft();
                break;
            }
        }
    }

    screen_rect_ = QRect(QPoint(0, 0), size);
    desktop_rect_ = desktop.isEmpty() ? screen_rect_ : QRect(QPoint(0, 0), desktop.size());
    current->setTopLeft(offset);
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerKwin::refreshOutputs()
{
    outputs_age_.restart();

    QString socket_path;
    for (const char* name : { "wayland-0", "wayland-1" })
    {
        const QString candidate = QString("/run/user/%1/").arg(session_uid_) + name;
        if (access(candidate.toLocal8Bit().constData(), F_OK) == 0)
        {
            socket_path = candidate;
            break;
        }
    }
    if (socket_path.isEmpty())
        return;

    const QList<WaylandOutputLayout::Output> outputs = WaylandOutputLayout::query(socket_path);
    if (!outputs.isEmpty())
        outputs_ = outputs;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerKwin::screenCount()
{
    // Called once per captured frame; re-read the layout on a throttle rather than on every call, since
    // querying Wayland opens a fresh socket connection each time.
    if (!outputs_age_.isValid() || outputs_age_.elapsed() > 2000)
        refreshOutputs();

    return outputs_.isEmpty() ? 1 : static_cast<int>(outputs_.size());
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKwin::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    refreshOutputs();

    if (outputs_.isEmpty())
    {
        // No layout available: expose the whole captured workspace as a single screen.
        Screen screen;
        screen.id = 0;
        screen.position = QPoint(0, 0);
        screen.resolution = screen_rect_.size();
        screen.dpi = QPoint(96, 96);
        screen.is_primary = true;
        screens->screens.append(screen);
        screens->resolutions.append(screen_rect_.size());
        return true;
    }

    for (const WaylandOutputLayout::Output& output : std::as_const(outputs_))
    {
        Screen screen;
        screen.id = outputId(output.name);
        screen.title = output.name;
        screen.position = output.logical.topLeft();
        screen.resolution = output.logical.size();
        screen.dpi = QPoint(96, 96);
        screens->screens.append(screen);
        screens->resolutions.append(output.logical.size());
    }

    // Exactly one screen is primary: prefer the one at the layout origin, else the first.
    int primary_index = 0;
    for (int i = 0; i < screens->screens.size(); ++i)
    {
        if (screens->screens[i].position == QPoint(0, 0))
        {
            primary_index = i;
            break;
        }
    }
    screens->screens[primary_index].is_primary = true;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKwin::selectScreen(ScreenId screen_id)
{
    refreshOutputs();

    if (outputs_.isEmpty())
    {
        // Single-screen fallback (whole workspace); nothing to switch.
        selected_output_.clear();
        return true;
    }

    for (const WaylandOutputLayout::Output& output : std::as_const(outputs_))
    {
        if (outputId(output.name) != screen_id)
            continue;

        if (selected_output_ != output.name)
        {
            selected_output_ = output.name;
            // Drop frames sized for the previously captured output.
            queue_.reset();
            differ_.reset();
        }

        LOG(INFO) << "KWin selected screen:" << output.name;
        return true;
    }

    LOG(ERROR) << "KWin selectScreen: unknown screen id" << screen_id;
    return false;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerKwin::currentScreen() const
{
    return selected_output_.isEmpty() ? 0 : outputId(selected_output_);
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerKwin::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();
    *error = Error::TEMPORARY;

    if (!capture())
        return nullptr;

    Frame* current = queue_.currentFrame();
    if (!current)
        return nullptr;

    // Report only the changed region. On the first frame after a (re)allocation there is nothing to
    // diff against, so the whole screen is reported.
    Frame* previous = queue_.previousFrame();
    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(current->size(), current->stride());
        *current->updatedRegion() = screen_rect_;
    }
    else
    {
        differ_->calcDirtyRegion(previous->frameData(), current->frameData(),
                                 current->updatedRegion());
    }

    *error = Error::SUCCEEDED;
    return current;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerKwin::captureCursor()
{
    if (drm_fd_ < 0 || !egl_dmabuf_)
        return nullptr;

    quint32 cursor_fb_id = 0;
    QSize cursor_size;
    QPoint hotspot;
    int crtc_width = 0;
    if (!findCursorPlane(&cursor_fb_id, &cursor_size, &cursor_position_, &hotspot, &crtc_width))
        return nullptr;

    // Re-read the shape only when the cursor plane's framebuffer changes.
    if (cursor_fb_id == last_cursor_fb_id_)
        return nullptr;
    last_cursor_fb_id_ = cursor_fb_id;

    drmModeFB2* fb = LibDrm::modeGetFB2(drm_fd_, cursor_fb_id);
    if (!fb || !fb->handles[0])
    {
        if (fb)
            LibDrm::modeFreeFB2(fb);
        return nullptr;
    }

    QByteArray image;
    image.resize(cursor_size.width() * cursor_size.height() * MouseCursor::kBytesPerPixel);

    const bool ok = readCursorFb(egl_dmabuf_.get(), drm_fd_, fb,
                                 reinterpret_cast<quint8*>(image.data()),
                                 cursor_size.width() * MouseCursor::kBytesPerPixel, cursor_size);
    LibDrm::modeFreeFB2(fb);

    if (!ok)
    {
        // Some virtual GPUs (e.g. vmwgfx) cannot export the cursor buffer for reading. Disable cursor
        // capture after the first failure so it does not spam the kernel log on every cursor change.
        LOG(WARNING) << "Cursor framebuffer import failed; disabling hardware cursor capture";
        egl_dmabuf_.reset();
        ::close(drm_fd_);
        drm_fd_ = -1;
        return nullptr;
    }

    // The cursor plane is captured at physical pixels, but the workspace frame from ScreenShot2 is in
    // logical coordinates. Downscale the cursor (and its hotspot) by the output scale (physical CRTC
    // width / logical workspace width) so it matches the apparent cursor size on the client.
    const double scale = (crtc_width > 0 && screen_rect_.width() > 0)
        ? static_cast<double>(crtc_width) / screen_rect_.width() : 1.0;
    if (scale > 1.0)
    {
        const QSize scaled(qMax(qRound(cursor_size.width() / scale), 1),
                           qMax(qRound(cursor_size.height() / scale), 1));

        QByteArray scaled_image;
        scaled_image.resize(scaled.width() * scaled.height() * MouseCursor::kBytesPerPixel);

        libyuv::ARGBScale(reinterpret_cast<const quint8*>(image.constData()),
                          cursor_size.width() * MouseCursor::kBytesPerPixel,
                          cursor_size.width(), cursor_size.height(),
                          reinterpret_cast<quint8*>(scaled_image.data()),
                          scaled.width() * MouseCursor::kBytesPerPixel,
                          scaled.width(), scaled.height(), libyuv::kFilterBox);

        image = std::move(scaled_image);
        cursor_size = scaled;
        hotspot = QPoint(qRound(hotspot.x() / scale), qRound(hotspot.y() / scale));
    }

    mouse_cursor_ = std::make_unique<MouseCursor>(std::move(image), cursor_size, hotspot);
    return mouse_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerKwin::cursorPosition()
{
    return cursor_position_;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKwin::findCursorPlane(quint32* fb_id, QSize* size, QPoint* position,
                                         QPoint* hotspot, int* crtc_width)
{
    drmModePlaneRes* plane_res = LibDrm::modeGetPlaneResources(drm_fd_);
    if (!plane_res)
        return false;

    bool found = false;

    // The cursor is a small framebuffer bound to a CRTC; the primary plane is screen-sized and the
    // overlay planes are normally inactive, so the small active plane is the hardware cursor. The whole
    // workspace is captured, so the cursor on any CRTC is on screen.
    for (uint32_t i = 0; i < plane_res->count_planes && !found; ++i)
    {
        drmModePlane* plane = LibDrm::modeGetPlane(drm_fd_, plane_res->planes[i]);
        if (!plane)
            continue;

        if (plane->fb_id && plane->crtc_id)
        {
            drmModeFB2* fb = LibDrm::modeGetFB2(drm_fd_, plane->fb_id);
            if (fb && fb->width && fb->height &&
                fb->width <= kMaxCursorSize && fb->height <= kMaxCursorSize)
            {
                if (fb_id)
                    *fb_id = plane->fb_id;
                if (size)
                    *size = QSize(static_cast<int>(fb->width), static_cast<int>(fb->height));
                if (position)
                    *position = QPoint(static_cast<int>(plane->crtc_x),
                                       static_cast<int>(plane->crtc_y));
                if (crtc_width)
                {
                    // Physical resolution of the cursor's CRTC, to derive the output scale.
                    drmModeCrtc* crtc = LibDrm::modeGetCrtc(drm_fd_, plane->crtc_id);
                    if (crtc)
                    {
                        *crtc_width = static_cast<int>(crtc->width);
                        LibDrm::modeFreeCrtc(crtc);
                    }
                }
                if (hotspot)
                {
                    // Paravirtual drivers (vmwgfx) report the cursor hotspot in plane properties rather
                    // than via the plane position.
                    drmModeObjectProperties* props = LibDrm::modeObjectGetProperties(
                        drm_fd_, plane->plane_id, DRM_MODE_OBJECT_PLANE);
                    if (props)
                    {
                        for (uint32_t p = 0; p < props->count_props; ++p)
                        {
                            drmModePropertyRes* prop = LibDrm::modeGetProperty(drm_fd_, props->props[p]);
                            if (!prop)
                                continue;

                            if (strcmp(prop->name, "HOTSPOT_X") == 0)
                                hotspot->setX(static_cast<int>(props->prop_values[p]));
                            else if (strcmp(prop->name, "HOTSPOT_Y") == 0)
                                hotspot->setY(static_cast<int>(props->prop_values[p]));

                            LibDrm::modeFreeProperty(prop);
                        }
                        LibDrm::modeFreeObjectProperties(props);
                    }
                }
                found = true;
            }
            if (fb)
                LibDrm::modeFreeFB2(fb);
        }

        LibDrm::modeFreePlane(plane);
    }

    LibDrm::modeFreePlaneResources(plane_res);
    return found;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerKwin::desktopRect() const
{
    // The full logical workspace (all outputs), so the absolute pointer is mapped over the whole desktop
    // even when a single output is captured. Falls back to the captured frame until the layout is known.
    return desktop_rect_.isEmpty() ? screen_rect_ : desktop_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerKwin::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerKwin::reset()
{
    queue_.reset();
    differ_.reset();
    mouse_cursor_.reset();
    last_cursor_fb_id_ = 0;
}
