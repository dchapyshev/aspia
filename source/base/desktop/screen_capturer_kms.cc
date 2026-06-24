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

#include "base/desktop/screen_capturer_kms.h"

#include <drm/drm_fourcc.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>

#include "base/logging.h"
#include "base/desktop/differ.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/linux/egl_dmabuf.h"
#include "base/linux/libdrm.h"

namespace {

const int kAlignment = 32;
const int kMaxCards = 8;
// A hardware cursor framebuffer is small; this tells it apart from the screen-sized primary plane.
const int kMaxCursorSize = 256;

//--------------------------------------------------------------------------------------------------
// Imports framebuffer |fb| into |dst| (|dst_stride| bytes per row) as packed BGRA via EGL/GBM. The
// GEM handles opened by drmModeGetFB2() are released here; the caller still owns and frees |fb|.
bool importFb(EglDmaBuf* egl_dmabuf, int drm_fd, drmModeFB2* fb, quint8* dst, int dst_stride)
{
    const QSize size(static_cast<int>(fb->width), static_cast<int>(fb->height));
    const quint32 fourcc = fb->pixel_format;
    const quint64 modifier =
        (fb->flags & DRM_MODE_FB_MODIFIERS) ? fb->modifier : DRM_FORMAT_MOD_INVALID;

    // Export each plane's GEM handle as a DMA-BUF descriptor for the EGL importer. Handles created by
    // drmModeGetFB2() live in our fd namespace and must be released afterwards.
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
            LibDrm::closeBufferHandle(drm_fd, fb->handles[i]);
    }

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

    return true;
}

//--------------------------------------------------------------------------------------------------
quint32 ScreenCapturerKms::activeFramebufferId()
{
    drmModeRes* resources = LibDrm::modeGetResources(drm_fd_);
    if (!resources)
        return 0;

    quint32 fb_id = 0;
    int total_width = 0;
    int max_height = 0;
    for (int i = 0; i < resources->count_crtcs; ++i)
    {
        drmModeCrtc* crtc = LibDrm::modeGetCrtc(drm_fd_, resources->crtcs[i]);
        if (crtc)
        {
            if (crtc->mode_valid && crtc->buffer_id)
            {
                // The compositor maps the absolute pointer over the whole logical desktop. Its layout
                // (monitor positions) is not exposed by DRM, so assume the outputs are laid out left
                // to right with the captured (first active) one at the origin.
                total_width += static_cast<int>(crtc->width);
                max_height = std::max(max_height, static_cast<int>(crtc->height));
                if (!fb_id)
                {
                    fb_id = crtc->buffer_id;
                    crtc_id_ = resources->crtcs[i];
                }
            }
            LibDrm::modeFreeCrtc(crtc);
        }
    }

    LibDrm::modeFreeResources(resources);

    if (total_width > 0 && max_height > 0)
        desktop_rect_ = QRect(0, 0, total_width, max_height);

    return fb_id;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerKms::screenCount()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    Screen screen;
    screen.id = 0;
    screen.position = QPoint(0, 0);
    screen.resolution = screen_rect_.size();
    screen.is_primary = true;

    screens->screens.append(screen);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerKms::selectScreen(ScreenId /* screen_id */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerKms::currentScreen() const
{
    return 0;
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

    const bool ok = current &&
        importFb(egl_dmabuf_.get(), drm_fd_, fb, current->frameData(), current->stride());

    LibDrm::modeFreeFB2(fb);

    if (!ok || !current)
        return nullptr;

    screen_rect_ = QRect(QPoint(0, 0), size);

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
bool ScreenCapturerKms::findCursorPlane(quint32* fb_id, QSize* size, QPoint* position)
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
const MouseCursor* ScreenCapturerKms::captureCursor()
{
    quint32 cursor_fb_id = 0;
    QSize cursor_size;
    if (!findCursorPlane(&cursor_fb_id, &cursor_size, &cursor_position_))
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

    const bool ok = importFb(egl_dmabuf_.get(), drm_fd_, fb, reinterpret_cast<quint8*>(image.data()),
                             cursor_size.width() * MouseCursor::kBytesPerPixel);
    LibDrm::modeFreeFB2(fb);

    if (!ok)
        return nullptr;

    // The cursor plane position is the image top-left on screen, reported via cursorPosition(), so
    // the shape carries a zero hotspot.
    mouse_cursor_ = std::make_unique<MouseCursor>(std::move(image), cursor_size, QPoint(0, 0));
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
}
