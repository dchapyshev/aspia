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

#include "host/screen_capturer_kms.h"

#include <QString>

#include <drm/drm_fourcc.h>
#include <libyuv/convert_argb.h>
#include <libyuv/planar_functions.h>

#include <fcntl.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/desktop/differ.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "base/linux/libsystemd.h"
#include "host/linux/egl_dmabuf.h"
#include "host/linux/libdrm.h"
#include "host/linux/wayland_output_layout.h"

namespace {

const int kAlignment = 32;
const int kMaxCards = 8;
// A hardware cursor framebuffer is small; this tells it apart from the screen-sized primary plane.
const int kMaxCursorSize = 256;

// One active CRTC, i.e. one lit-up monitor selectable for capture.
struct Monitor
{
    quint32 crtc_id = 0;
    quint32 fb_id = 0;
    QSize mode_size;
    QString connector;
};

//--------------------------------------------------------------------------------------------------
// Returns the kernel connector type name as compositors use it (e.g. "eDP", "HDMI-A", "DP"). The
// output name a compositor reports is this plus "-" plus the connector's per-type id (e.g. "eDP-1").
const char* connectorTypeName(uint32_t type)
{
    switch (type)
    {
        case DRM_MODE_CONNECTOR_VGA: return "VGA";
        case DRM_MODE_CONNECTOR_DVII: return "DVI-I";
        case DRM_MODE_CONNECTOR_DVID: return "DVI-D";
        case DRM_MODE_CONNECTOR_DVIA: return "DVI-A";
        case DRM_MODE_CONNECTOR_Composite: return "Composite";
        case DRM_MODE_CONNECTOR_SVIDEO: return "SVIDEO";
        case DRM_MODE_CONNECTOR_LVDS: return "LVDS";
        case DRM_MODE_CONNECTOR_Component: return "Component";
        case DRM_MODE_CONNECTOR_9PinDIN: return "DIN";
        case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
        case DRM_MODE_CONNECTOR_HDMIA: return "HDMI-A";
        case DRM_MODE_CONNECTOR_HDMIB: return "HDMI-B";
        case DRM_MODE_CONNECTOR_TV: return "TV";
        case DRM_MODE_CONNECTOR_eDP: return "eDP";
        case DRM_MODE_CONNECTOR_VIRTUAL: return "Virtual";
        case DRM_MODE_CONNECTOR_DSI: return "DSI";
        case DRM_MODE_CONNECTOR_DPI: return "DPI";
        case DRM_MODE_CONNECTOR_WRITEBACK: return "Writeback";
        case DRM_MODE_CONNECTOR_SPI: return "SPI";
        case DRM_MODE_CONNECTOR_USB: return "USB";
        default: return "Unknown";
    }
}

//--------------------------------------------------------------------------------------------------
// Returns the connector name driving |crtc_id| (e.g. "eDP-1") using the already-fetched |resources|:
// the connector whose currently bound encoder points at the CRTC. Empty if none does.
QString connectorNameForCrtc(int drm_fd, drmModeRes* resources, quint32 crtc_id)
{
    if (!crtc_id || !resources)
        return QString();

    QString name;
    for (int i = 0; i < resources->count_connectors && name.isEmpty(); ++i)
    {
        drmModeConnector* connector =
            LibDrm::modeGetConnectorCurrent(drm_fd, resources->connectors[i]);
        if (!connector)
            continue;

        // The connector drives the CRTC through its currently bound encoder.
        if (connector->encoder_id)
        {
            drmModeEncoder* encoder = LibDrm::modeGetEncoder(drm_fd, connector->encoder_id);
            if (encoder)
            {
                if (encoder->crtc_id == crtc_id)
                {
                    name = QString("%1-%2").arg(connectorTypeName(connector->connector_type))
                                           .arg(connector->connector_type_id);
                }
                LibDrm::modeFreeEncoder(encoder);
            }
        }

        LibDrm::modeFreeConnector(connector);
    }

    return name;
}

//--------------------------------------------------------------------------------------------------
// Counts the CRTCs currently scanning out a framebuffer (one per lit-up monitor). Cheap enough to
// call on every captured frame: it issues only DRM ioctls and resolves no connector names.
int activeCrtcCount(int drm_fd)
{
    drmModeRes* resources = LibDrm::modeGetResources(drm_fd);
    if (!resources)
        return 0;

    int active = 0;
    for (int i = 0; i < resources->count_crtcs; ++i)
    {
        drmModeCrtc* crtc = LibDrm::modeGetCrtc(drm_fd, resources->crtcs[i]);
        if (crtc)
        {
            if (crtc->mode_valid && crtc->buffer_id)
                ++active;
            LibDrm::modeFreeCrtc(crtc);
        }
    }

    LibDrm::modeFreeResources(resources);
    return active;
}

//--------------------------------------------------------------------------------------------------
// Enumerates every active CRTC with its connector name and current mode size. Each entry is one
// monitor selectable for capture; its CRTC id is the stable screen id reported to the client.
QList<Monitor> activeMonitors(int drm_fd)
{
    QList<Monitor> monitors;

    drmModeRes* resources = LibDrm::modeGetResources(drm_fd);
    if (!resources)
        return monitors;

    for (int i = 0; i < resources->count_crtcs; ++i)
    {
        drmModeCrtc* crtc = LibDrm::modeGetCrtc(drm_fd, resources->crtcs[i]);
        if (crtc)
        {
            if (crtc->mode_valid && crtc->buffer_id)
            {
                Monitor monitor;
                monitor.crtc_id = resources->crtcs[i];
                monitor.fb_id = crtc->buffer_id;
                monitor.mode_size =
                    QSize(static_cast<int>(crtc->width), static_cast<int>(crtc->height));
                monitor.connector = connectorNameForCrtc(drm_fd, resources, monitor.crtc_id);
                monitors.append(monitor);
            }
            LibDrm::modeFreeCrtc(crtc);
        }
    }

    LibDrm::modeFreeResources(resources);
    return monitors;
}

//--------------------------------------------------------------------------------------------------
// Reads the active seat compositor's logical output layout over Wayland (positions and logical sizes,
// fractional scale folded in). Empty when no active Wayland session is reachable.
QList<WaylandOutputLayout::Output> queryCompositorOutputs()
{
    char* session = nullptr;
    uid_t uid = 0;
    if (LibSystemd::seatGetActive("seat0", &session, &uid) < 0 || !session)
        return {};
    free(session);

    QString socket_path;
    for (const char* name : { "wayland-0", "wayland-1" })
    {
        const QString candidate = QString("/run/user/%1/").arg(uid) + name;
        if (access(candidate.toLocal8Bit().constData(), F_OK) == 0)
        {
            socket_path = candidate;
            break;
        }
    }
    if (socket_path.isEmpty())
        return {};

    return WaylandOutputLayout::query(socket_path);
}

//--------------------------------------------------------------------------------------------------
bool convertToBgra(const quint8* src, int src_stride, const drmModeFB2* fb, quint8* dst, int dst_stride)
{
    const int width = static_cast<int>(fb->width);
    const int height = static_cast<int>(fb->height);
    int rc = -1;
    switch (fb->pixel_format)
    {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_ARGB8888:  // memory B,G,R,A - already the target layout
            rc = libyuv::ARGBCopy(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_ABGR8888:  // memory R,G,B,A
            rc = libyuv::ABGRToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_RGBA8888:  // memory A,B,G,R
            rc = libyuv::RGBAToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_BGRX8888:
        case DRM_FORMAT_BGRA8888:  // memory A,R,G,B
            rc = libyuv::BGRAToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_ARGB2101010:  // 2-bit alpha + 10-bit R,G,B
            rc = libyuv::AR30ToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_ABGR2101010:  // 2-bit alpha + 10-bit B,G,R
            rc = libyuv::AB30ToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        case DRM_FORMAT_RGB565:
            rc = libyuv::RGB565ToARGB(src, src_stride, dst, dst_stride, width, height);
            break;
        default:
            LOG(ERROR) << "Unsupported scan-out pixel format for CPU readback:"
                       << Qt::hex << fb->pixel_format;
            break;
    }
    return rc == 0;
}

//--------------------------------------------------------------------------------------------------
// Reads a single-plane scan-out buffer into |dst| (packed BGRA) through a CPU mapping of its exported
// DMA-BUF. Used when the EGL/GL readback is unavailable: a VM whose 3D acceleration is inactive runs on
// software GL, which cannot read the hardware scan-out buffer.
bool readDmaBufCpu(int dmabuf_fd, const drmModeFB2* fb, quint8* dst, int dst_stride)
{
    const size_t map_size = fb->offsets[0] + static_cast<size_t>(fb->pitches[0]) * fb->height;

    void* map = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
    if (map == MAP_FAILED)
        return false;

    // Bracket the read with a DMA-BUF sync so the CPU sees coherent data (best effort; ignore failures).
    dma_buf_sync sync = {};
    sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
    ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);

    const bool ok = convertToBgra(static_cast<const quint8*>(map) + fb->offsets[0],
                                  static_cast<int>(fb->pitches[0]), fb, dst, dst_stride);

    sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
    ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
    munmap(map, map_size);
    return ok;
}

//--------------------------------------------------------------------------------------------------
// Reads a single-plane scan-out buffer into |dst| (packed BGRA) by mapping its GEM handle directly as a
// dumb buffer. Last-resort CPU path for drivers that cannot PRIME-export the scan-out at all (e.g.
// vmwgfx on a software VM), where both the EGL and the DMA-BUF paths are impossible.
bool readDumbBufferCpu(int drm_fd, const drmModeFB2* fb, quint8* dst, int dst_stride)
{
    if (!fb->handles[0])
        return false;

    uint64_t map_offset = 0;
    if (LibDrm::mapDumbBuffer(drm_fd, fb->handles[0], &map_offset) != 0)
        return false;

    const size_t map_size = static_cast<size_t>(fb->pitches[0]) * fb->height;
    void* map = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, drm_fd, static_cast<off_t>(map_offset));
    if (map == MAP_FAILED)
        return false;

    const bool ok = convertToBgra(static_cast<const quint8*>(map),
                                  static_cast<int>(fb->pitches[0]), fb, dst, dst_stride);
    munmap(map, map_size);
    return ok;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerKms::ScreenCapturerKms(QObject* parent)
    : ScreenCapturer(Type::LINUX_KMS, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerKms::~ScreenCapturerKms()
{
    if (drm_fd_ >= 0)
        ::close(drm_fd_);
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerKms* ScreenCapturerKms::create(QObject* parent)
{
    std::unique_ptr<ScreenCapturerKms> self(new ScreenCapturerKms(parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerKms::isAvailable()
{
    std::unique_ptr<ScreenCapturerKms> self(new ScreenCapturerKms());
    return self->init();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerKms::screenCount()
{
    return activeCrtcCount(drm_fd_);
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    const QList<Monitor> monitors = activeMonitors(drm_fd_);
    if (monitors.isEmpty())
    {
        // No active CRTC yet: expose a single synthetic screen covering the captured framebuffer so
        // the client always has something to select.
        Screen screen;
        screen.id = static_cast<ScreenId>(crtc_id_);
        screen.position = QPoint(0, 0);
        screen.resolution = screen_rect_.size();
        screen.dpi = QPoint(96, 96);
        screen.is_primary = true;
        screens->screens.append(screen);
        return true;
    }

    // Positions come from the compositor's logical layout, matched to each CRTC by connector name (the
    // captured-buffer size from DRM is the resolution). DRM alone has no cross-monitor layout.
    const QList<WaylandOutputLayout::Output> outputs = queryCompositorOutputs();

    // When the layout is unavailable (no compositor reachable), tile the monitors left to right so
    // they do not all collapse onto the same origin.
    QPoint fallback_position(0, 0);

    for (const Monitor& monitor : std::as_const(monitors))
    {
        QRect logical;
        for (const WaylandOutputLayout::Output& output : std::as_const(outputs))
        {
            if (!monitor.connector.isEmpty() && output.name == monitor.connector)
            {
                logical = output.logical;
                break;
            }
        }

        Screen screen;
        screen.id = static_cast<ScreenId>(monitor.crtc_id);
        screen.resolution = monitor.mode_size;
        screen.dpi = QPoint(96, 96);
        screen.title = monitor.connector.isEmpty() ?
            QString("Screen %1").arg(monitor.crtc_id) : monitor.connector;

        if (!logical.isEmpty())
        {
            screen.position = logical.topLeft();
        }
        else
        {
            screen.position = fallback_position;
            fallback_position.rx() += monitor.mode_size.width();
        }

        screens->screens.append(screen);
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
bool ScreenCapturerKms::selectScreen(ScreenId screen_id)
{
    const QList<Monitor> monitors = activeMonitors(drm_fd_);

    // The capturer may have no active CRTC yet (created before the compositor lit an output). Accept
    // the request and bind it on the first frame that finds the CRTC active.
    if (monitors.isEmpty())
    {
        selected_crtc_id_ = static_cast<quint32>(screen_id);
        queue_.reset();
        input_geometry_valid_ = false;
        input_geometry_attempts_ = 0;
        return true;
    }

    for (const Monitor& monitor : std::as_const(monitors))
    {
        if (static_cast<ScreenId>(monitor.crtc_id) == screen_id)
        {
            selected_crtc_id_ = monitor.crtc_id;

            // Drop frames sized for the previously selected monitor and re-learn the input geometry
            // for the new one.
            queue_.reset();
            input_geometry_valid_ = false;
            input_geometry_attempts_ = 0;

            LOG(INFO) << "KMS selected screen: CRTC" << monitor.crtc_id << monitor.connector;
            return true;
        }
    }

    LOG(ERROR) << "KMS selectScreen: CRTC" << screen_id << "is not an active monitor";
    return false;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerKms::currentScreen() const
{
    return static_cast<ScreenId>(selected_crtc_id_ ? selected_crtc_id_ : crtc_id_);
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerKms::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();
    *error = Error::TEMPORARY;

    const quint32 fb_id = activeFramebufferId();
    if (!fb_id)
        return nullptr;

    drmModeFB2* fb = LibDrm::modeGetFB2(drm_fd_, fb_id);
    if (!fb || !fb->width || !fb->height || !fb->handles[0])
    {
        if (fb)
            LibDrm::modeFreeFB2(fb);
        return nullptr;
    }

    const QSize size(static_cast<int>(fb->width), static_cast<int>(fb->height));
    // Capture alternately into the two queue frames so the previous one can be diffed against.
    if (!queue_.currentFrame() || queue_.currentFrame()->size() != size)
    {
        std::unique_ptr<Frame> frame = FrameAligned::create(size, kAlignment);
        if (frame)
        {
            frame->setCapturerType(static_cast<quint32>(type()));
            queue_.replaceCurrentFrame(std::move(frame));
        }
    }

    Frame* current = queue_.currentFrame();

    const bool ok = current && importFb(fb, current->frameData(), current->stride());

    LibDrm::modeFreeFB2(fb);

    if (!ok || !current)
        return nullptr;

    screen_rect_ = QRect(QPoint(0, 0), size);

    // Learn the input-mapping geometry (screen size + this monitor's offset, scale folded in) from the
    // compositor's logical layout, lazily and with a few retries until the compositor is reachable.
    // The frame's top-left carries the offset that the agent passes to the injector via setScreenInfo.
    if (!input_geometry_valid_ && input_geometry_attempts_ < 30)
    {
        ++input_geometry_attempts_;
        updateInputGeometry(size);
    }
    current->setTopLeft(capture_offset_);

    // Report only the changed region. On the first frame after a (re)allocation there is nothing to
    // diff against, so the whole screen is reported.
    Frame* previous = queue_.previousFrame();
    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(size, current->stride());
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
const MouseCursor* ScreenCapturerKms::captureCursor()
{
    quint32 cursor_fb_id = 0;
    QSize cursor_size;
    QPoint hotspot;
    if (!findCursorPlane(&cursor_fb_id, &cursor_size, &cursor_position_, &hotspot))
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

    const bool ok = importFb(fb, reinterpret_cast<quint8*>(image.data()),
                             cursor_size.width() * MouseCursor::kBytesPerPixel);
    LibDrm::modeFreeFB2(fb);

    if (!ok)
        return nullptr;

    // Paravirtual drivers (vmwgfx) place the cursor plane at the pointer position and report the
    // hotspot in plane properties; standard drivers place the image top-left and report no hotspot.
    // Either way the client draws the image so the hotspot lands on cursorPosition().
    mouse_cursor_ = std::make_unique<MouseCursor>(std::move(image), cursor_size, hotspot);
    return mouse_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerKms::cursorPosition()
{
    return cursor_position_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerKms::desktopRect() const
{
    // The full logical desktop (all active CRTCs), so the absolute pointer is scaled to the area the
    // compositor maps it over. Falls back to the captured screen until the first frame computes it.
    return desktop_rect_.isEmpty() ? screen_rect_ : desktop_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerKms::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerKms::reset()
{
    queue_.reset();
    differ_.reset();
    mouse_cursor_.reset();
    last_cursor_fb_id_ = 0;
    input_geometry_valid_ = false;
    input_geometry_attempts_ = 0;
    active_crtc_count_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::init()
{
    if (!LibDrm::ensureLoaded())
        return false;

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
            LOG(INFO) << "KMS capture device:" << path.constData();
            // Never hold DRM master: the compositor (e.g. the greeter's gnome-shell) must be able to
            // acquire it for modesetting. Opening a DRM node can implicitly grant master to the first
            // client, which would block the compositor and leave the physical screen black. With
            // CAP_SYS_ADMIN, drmModeGetFB2() reads the scanout buffer without holding master.
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
        LOG(ERROR) << "No KMS-capable DRM device found";
        return false;
    }

    // Enumerate all planes (primary, cursor, overlay), not just overlays, so the hardware cursor
    // plane can be found in captureCursor().
    if (LibDrm::setClientCap(drm_fd_, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
        LOG(INFO) << "DRM universal planes unavailable; hardware cursor will not be captured";

    egl_dmabuf_ = std::make_unique<EglDmaBuf>();
    if (!egl_dmabuf_->isInitialized())
    {
        LOG(ERROR) << "EGL/GBM import not available for KMS capture";
        return false;
    }

    // Confirm the active scan-out can really be read (so we never commit to a KMS capturer that yields
    // only black frames) and fix the readback method before the first real frame.
    if (!probeReadback())
    {
        LOG(ERROR) << "KMS scan-out readback probe failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::probeReadback()
{
    const quint32 fb_id = activeFramebufferId();
    if (!fb_id)
        return false;

    // GL readback vs a CPU mapping is driver-specific, so the working method can only be found by
    // attempting a real import. Try each candidate once on a fresh framebuffer (importFb consumes the
    // GEM handles) and keep the first that succeeds for the rest of the session.
    static constexpr Readback kMethods[] =
        { Readback::EGL, Readback::DMABUF_CPU, Readback::DUMB_CPU };

    for (Readback method : kMethods)
    {
        const char* name = (method == Readback::EGL)        ? "EGL" :
                           (method == Readback::DMABUF_CPU) ? "CPU DMA-BUF mapping" :
                                                              "CPU dumb-buffer mapping";

        drmModeFB2* fb = LibDrm::modeGetFB2(drm_fd_, fb_id);
        if (!fb || !fb->width || !fb->height || !fb->handles[0])
        {
            LOG(ERROR) << "KMS probe: framebuffer" << fb_id << "not readable: handle0="
                       << (fb ? fb->handles[0] : 0) << "size="
                       << (fb ? fb->width : 0) << "x" << (fb ? fb->height : 0) << "format="
                       << (fb ? fb->pixel_format : 0);
            if (fb)
                LibDrm::modeFreeFB2(fb);
            return false;
        }

        const int stride = static_cast<int>(fb->width) * 4;
        QByteArray scratch(static_cast<qsizetype>(stride) * fb->height, Qt::Uninitialized);

        readback_ = method;
        const bool ok = importFb(fb, reinterpret_cast<quint8*>(scratch.data()), stride);
        LibDrm::modeFreeFB2(fb);

        if (ok)
        {
            LOG(INFO) << "KMS readback method:" << name;
            return true;
        }

        LOG(INFO) << "KMS probe: readback method" << name << "did not work, trying next";
    }

    readback_ = Readback::UNKNOWN;
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::importFb(drmModeFB2* fb, quint8* dst, int dst_stride)
{
    bool ok = false;

    if (readback_ == Readback::DUMB_CPU)
    {
        // The driver cannot PRIME-export the scan-out: map the GEM handle as a dumb buffer instead.
        ok = readDumbBufferCpu(drm_fd_, fb, dst, dst_stride);
    }
    else
    {
        // The EGL and CPU DMA-BUF paths both need the scan-out exported as a DMA-BUF first. Handles
        // created by drmModeGetFB2() live in our fd namespace and are released at the end.
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
            if (LibDrm::primeHandleToFD(drm_fd_, fb->handles[i], DRM_CLOEXEC, &prime_fd) != 0 ||
                prime_fd < 0)
            {
                break;
            }

            prime_fds[plane_count] = prime_fd;
            planes[plane_count].fd = prime_fd;
            planes[plane_count].offset = fb->offsets[i];
            planes[plane_count].stride = fb->pitches[i];
            ++plane_count;
        }

        if (readback_ == Readback::EGL && plane_count > 0)
        {
            ok = egl_dmabuf_->imageFromDmaBuf(size, fourcc, planes.data(), plane_count, modifier,
                                              QRect(QPoint(0, 0), size), dst, dst_stride);
        }
        else if (readback_ == Readback::DMABUF_CPU && plane_count == 1 && prime_fds[0] >= 0)
        {
            ok = readDmaBufCpu(prime_fds[0], fb, dst, dst_stride);
        }

        for (int i = 0; i < plane_count; ++i)
        {
            if (prime_fds[i] >= 0)
                ::close(prime_fds[i]);
        }
    }

    // Close the unique GEM handles opened by drmModeGetFB2().
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
            LibDrm::closeBufferHandle(drm_fd_, fb->handles[i]);
    }

    return ok;
}

//--------------------------------------------------------------------------------------------------
quint32 ScreenCapturerKms::activeFramebufferId()
{
    drmModeRes* resources = LibDrm::modeGetResources(drm_fd_);
    if (!resources)
        return 0;

    quint32 first_fb_id = 0;
    quint32 first_crtc_id = 0;
    quint32 selected_fb_id = 0;
    int active = 0;
    for (int i = 0; i < resources->count_crtcs; ++i)
    {
        drmModeCrtc* crtc = LibDrm::modeGetCrtc(drm_fd_, resources->crtcs[i]);
        if (crtc)
        {
            if (crtc->mode_valid && crtc->buffer_id)
            {
                ++active;
                if (!first_fb_id)
                {
                    first_fb_id = crtc->buffer_id;
                    first_crtc_id = resources->crtcs[i];
                }
                if (selected_crtc_id_ && resources->crtcs[i] == selected_crtc_id_)
                    selected_fb_id = crtc->buffer_id;
            }
            LibDrm::modeFreeCrtc(crtc);
        }
    }

    LibDrm::modeFreeResources(resources);

    // A change in the active-CRTC count means a monitor was connected or disconnected, so the logical
    // layout changed: re-read the input geometry.
    if (active != active_crtc_count_)
    {
        active_crtc_count_ = active;
        input_geometry_valid_ = false;
        input_geometry_attempts_ = 0;
    }

    // Capture the client-selected monitor; fall back to the first active CRTC when nothing is selected
    // yet or the selected monitor is no longer lit up.
    if (selected_fb_id)
    {
        crtc_id_ = selected_crtc_id_;
        return selected_fb_id;
    }

    crtc_id_ = first_crtc_id;
    return first_fb_id;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerKms::updateInputGeometry(const QSize& captured)
{
    // Fallback: map over the captured monitor alone (single output, no offset, no scale).
    desktop_rect_ = QRect(QPoint(0, 0), captured);
    capture_offset_ = QPoint(0, 0);

    if (captured.isEmpty())
        return;

    // The active seat session's compositor exposes the logical monitor layout over Wayland (readable
    // by a root process even on the login screen). DRM has no logical layout, so this is the source.
    const QList<WaylandOutputLayout::Output> outputs = queryCompositorOutputs();
    if (outputs.isEmpty())
        return;

    // The bounding box of all logical outputs is the area the compositor maps the absolute pointer
    // over.
    QRect logical_desktop;
    for (const WaylandOutputLayout::Output& output : outputs)
        logical_desktop = logical_desktop.united(output.logical);

    // Match the captured CRTC to its output by connector name: this stays correct even when two
    // monitors share a physical size (where a size match would be ambiguous). Fall back to a size
    // match only if the connector name cannot be resolved or does not appear in the layout.
    const QString connector = capturedConnectorName();
    QRect captured_logical;
    for (const WaylandOutputLayout::Output& output : outputs)
    {
        if (!connector.isEmpty() && output.name == connector)
        {
            captured_logical = output.logical;
            break;
        }
    }
    if (captured_logical.isEmpty())
    {
        for (const WaylandOutputLayout::Output& output : outputs)
        {
            if (output.physical == captured)
            {
                captured_logical = output.logical;
                break;
            }
        }
    }
    if (logical_desktop.isEmpty() || captured_logical.isEmpty())
        return;

    // Express the logical desktop and this monitor's offset in the captured monitor's physical pixels
    // (= logical * scale). The injector's plain (pixel + offset) / size mapping then lands captured-
    // physical pixels on the right logical point, fractional scale included.
    const double scale_x = static_cast<double>(captured.width()) / captured_logical.width();
    const double scale_y = static_cast<double>(captured.height()) / captured_logical.height();

    desktop_rect_ = QRect(0, 0,
                          qRound(scale_x * logical_desktop.width()),
                          qRound(scale_y * logical_desktop.height()));
    capture_offset_ = QPoint(qRound(scale_x * captured_logical.x()),
                             qRound(scale_y * captured_logical.y()));
    input_geometry_valid_ = true;

    LOG(INFO) << "KMS input geometry: connector" << connector << "captured" << captured << "logical"
              << captured_logical << "logical-desktop" << logical_desktop << "-> screen"
              << desktop_rect_.size() << "offset" << capture_offset_;
}

//--------------------------------------------------------------------------------------------------
QString ScreenCapturerKms::capturedConnectorName()
{
    if (!crtc_id_)
        return QString();

    drmModeRes* resources = LibDrm::modeGetResources(drm_fd_);
    if (!resources)
        return QString();

    const QString name = connectorNameForCrtc(drm_fd_, resources, crtc_id_);
    LibDrm::modeFreeResources(resources);
    return name;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::findCursorPlane(quint32* fb_id, QSize* size, QPoint* position, QPoint* hotspot)
{
    drmModePlaneRes* plane_res = LibDrm::modeGetPlaneResources(drm_fd_);
    if (!plane_res)
        return false;

    bool found = false;

    // The cursor is a small framebuffer bound to a CRTC; the primary plane is screen-sized and the
    // overlay planes are normally inactive, so the small active plane is the hardware cursor. Only
    // the cursor on the captured CRTC counts - on another output it is not on the screen we serve.
    for (uint32_t i = 0; i < plane_res->count_planes && !found; ++i)
    {
        drmModePlane* plane = LibDrm::modeGetPlane(drm_fd_, plane_res->planes[i]);
        if (!plane)
            continue;

        const bool on_captured_crtc =
            plane->crtc_id && (crtc_id_ == 0 || plane->crtc_id == crtc_id_);
        if (plane->fb_id && on_captured_crtc)
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
