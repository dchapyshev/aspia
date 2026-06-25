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

#include "base/desktop/screen_capturer_wlr.h"

#include <wayland-client.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "base/desktop/differ.h"
#include "base/desktop/frame_aligned.h"

#include "wlr-screencopy-unstable-v1-client-protocol.h"

namespace {

const int kAlignment = 32;

//--------------------------------------------------------------------------------------------------
// Connects to |uid|'s Wayland socket directly (no per-uid auth on the socket, unlike D-Bus).
wl_display* connectDisplay(uid_t uid)
{
    for (const char* name : { "wayland-0", "wayland-1" })
    {
        char path[128];
        std::snprintf(path, sizeof(path), "/run/user/%u/%s", static_cast<unsigned>(uid), name);
        if (access(path, F_OK) != 0)
            continue;

        int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
            continue;

        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            ::close(fd);
            continue;
        }

        // wl_display_connect_to_fd() takes ownership of the descriptor.
        wl_display* display = wl_display_connect_to_fd(fd);
        if (display)
            return display;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void registryGlobalRemove(void*, wl_registry*, uint32_t)
{
    // Not used.
}

//--------------------------------------------------------------------------------------------------
void availabilityGlobal(void* data, wl_registry*, uint32_t, const char* interface, uint32_t)
{
    if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0)
        *static_cast<bool*>(data) = true;
}

//--------------------------------------------------------------------------------------------------
void frameDamage(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t, uint32_t)
{
    // Not used (the whole frame is diffed locally).
}

//--------------------------------------------------------------------------------------------------
void frameLinuxDmabuf(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t)
{
    // Not used; the shm path is taken.
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerWlr::ScreenCapturerWlr(uid_t session_uid, QObject* parent)
    : ScreenCapturer(Type::LINUX_WLR, parent),
      session_uid_(session_uid)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerWlr::~ScreenCapturerWlr()
{
    disconnect();
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerWlr::isAvailable(uid_t session_uid)
{
    wl_display* display = connectDisplay(session_uid);
    if (!display)
        return false;

    bool available = false;
    wl_registry* registry = wl_display_get_registry(display);
    const wl_registry_listener listener = { availabilityGlobal, registryGlobalRemove };
    wl_registry_add_listener(registry, &listener, &available);
    wl_display_roundtrip(display);

    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return available;
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerWlr* ScreenCapturerWlr::create(uid_t session_uid, QObject* parent)
{
    std::unique_ptr<ScreenCapturerWlr> self(new ScreenCapturerWlr(session_uid, parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerWlr::screenCount()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerWlr::screenList(ScreenList* screens)
{
    Screen screen;
    screen.id = 0;
    screen.position = QPoint(0, 0);
    screen.resolution = screen_rect_.size();
    screen.is_primary = true;

    screens->screens.append(screen);
    screens->resolutions.append(screen_rect_.size());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerWlr::selectScreen(ScreenId /* screen_id */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerWlr::currentScreen() const
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerWlr::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();
    *error = Error::TEMPORARY;

    if (!capture())
        return nullptr;

    Frame* current = queue_.currentFrame();
    if (!current)
        return nullptr;

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
const MouseCursor* ScreenCapturerWlr::captureCursor()
{
    // The frame is captured without the cursor; a separate cursor source is a later refinement.
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerWlr::cursorPosition()
{
    return QPoint();
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerWlr::desktopRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerWlr::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::reset()
{
    queue_.reset();
    differ_.reset();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerWlr::init()
{
    display_ = connectDisplay(session_uid_);
    if (!display_)
    {
        LOG(ERROR) << "Unable to connect to the Wayland socket";
        return false;
    }

    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &kRegistryListener, this);
    wl_display_roundtrip(display_);

    if (!manager_ || !shm_ || !output_)
    {
        LOG(ERROR) << "wlr-screencopy/shm/output not available";
        return false;
    }

    // Prime the geometry and the first frame so screenList() is valid before the capture loop.
    Error error = Error::TEMPORARY;
    if (!captureFrame(&error) || error != Error::SUCCEEDED)
    {
        LOG(ERROR) << "Initial wlr-screencopy capture failed";
        return false;
    }

    LOG(INFO) << "wlr-screencopy capturer initialized, output:" << screen_rect_.size();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerWlr::capture()
{
    ready_ = false;
    failed_ = false;
    y_invert_ = false;
    frame_width_ = 0;

    zwlr_screencopy_frame_v1* frame = zwlr_screencopy_manager_v1_capture_output(manager_, 0, output_);
    if (!frame)
        return false;

    zwlr_screencopy_frame_v1_add_listener(frame, &kFrameListener, this);

    // The compositor advertises the buffer parameters right after capture_output; one roundtrip
    // delivers them (and buffer_done on v3).
    if (wl_display_roundtrip(display_) < 0 || failed_ || frame_width_ <= 0 || frame_stride_ <= 0)
    {
        zwlr_screencopy_frame_v1_destroy(frame);
        return false;
    }

    if (!ensureBuffer(frame_format_, frame_width_, frame_height_, frame_stride_))
    {
        zwlr_screencopy_frame_v1_destroy(frame);
        return false;
    }

    zwlr_screencopy_frame_v1_copy(frame, buffer_);

    while (!ready_ && !failed_)
    {
        if (wl_display_dispatch(display_) < 0)
        {
            failed_ = true;
            break;
        }
    }

    zwlr_screencopy_frame_v1_destroy(frame);

    if (!ready_)
        return false;

    const QSize size(frame_width_, frame_height_);
    if (!queue_.currentFrame() || queue_.currentFrame()->size() != size)
    {
        std::unique_ptr<Frame> frame_buffer = FrameAligned::create(size, kAlignment);
        if (!frame_buffer)
            return false;
        frame_buffer->setCapturerType(static_cast<quint32>(type()));
        queue_.replaceCurrentFrame(std::move(frame_buffer));
    }

    Frame* current = queue_.currentFrame();
    const quint8* src = static_cast<const quint8*>(shm_data_);
    quint8* dst = current->frameData();
    const int dst_stride = current->stride();
    const int row_bytes = std::min(frame_width_ * 4, frame_stride_);

    // The shm format is XRGB8888/ARGB8888 (packed BGRA in memory) - copy straight, flipping rows when
    // the compositor reports the buffer as y-inverted.
    for (int y = 0; y < frame_height_; ++y)
    {
        const int src_y = y_invert_ ? (frame_height_ - 1 - y) : y;
        memcpy(dst + y * dst_stride, src + src_y * frame_stride_, row_bytes);
    }

    screen_rect_ = QRect(QPoint(0, 0), size);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerWlr::ensureBuffer(uint32_t format, int width, int height, int stride)
{
    if (buffer_ && buffer_format_ == format && buffer_width_ == width &&
        buffer_height_ == height && buffer_stride_ == stride)
    {
        return true;
    }

    releaseBuffer();

    const size_t size = static_cast<size_t>(stride) * height;

    int fd = memfd_create("aspia-wlr", MFD_CLOEXEC);
    if (fd < 0)
    {
        PLOG(ERROR) << "memfd_create failed";
        return false;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0)
    {
        PLOG(ERROR) << "ftruncate failed";
        ::close(fd);
        return false;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        PLOG(ERROR) << "mmap failed";
        ::close(fd);
        return false;
    }

    wl_shm_pool* pool = wl_shm_create_pool(shm_, fd, static_cast<int32_t>(size));
    buffer_ = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_shm_pool_destroy(pool);

    if (!buffer_)
    {
        munmap(data, size);
        ::close(fd);
        return false;
    }

    shm_fd_ = fd;
    shm_data_ = data;
    shm_size_ = size;
    buffer_format_ = format;
    buffer_width_ = width;
    buffer_height_ = height;
    buffer_stride_ = stride;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::releaseBuffer()
{
    if (buffer_)
    {
        wl_buffer_destroy(buffer_);
        buffer_ = nullptr;
    }
    if (shm_data_)
    {
        munmap(shm_data_, shm_size_);
        shm_data_ = nullptr;
    }
    if (shm_fd_ >= 0)
    {
        ::close(shm_fd_);
        shm_fd_ = -1;
    }
    shm_size_ = 0;
    buffer_format_ = 0;
    buffer_width_ = buffer_height_ = buffer_stride_ = 0;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::disconnect()
{
    releaseBuffer();
    if (display_)
    {
        // Destroys all bound proxies (manager/shm/output/registry) with the connection.
        wl_display_disconnect(display_);
        display_ = nullptr;
    }
    registry_ = nullptr;
    manager_ = nullptr;
    shm_ = nullptr;
    output_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::registryGlobal(void* data, wl_registry* registry, uint32_t name,
                                       const char* interface, uint32_t version)
{
    static_cast<ScreenCapturerWlr*>(data)->onRegistryGlobal(registry, name, interface, version);
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::frameBuffer(void* data, zwlr_screencopy_frame_v1*, uint32_t format,
                                    uint32_t width, uint32_t height, uint32_t stride)
{
    static_cast<ScreenCapturerWlr*>(data)->onBuffer(format, width, height, stride);
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::frameFlags(void* data, zwlr_screencopy_frame_v1*, uint32_t flags)
{
    static_cast<ScreenCapturerWlr*>(data)->onFlags(flags);
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::frameReady(void* data, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t)
{
    static_cast<ScreenCapturerWlr*>(data)->onReady();
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::frameFailed(void* data, zwlr_screencopy_frame_v1*)
{
    static_cast<ScreenCapturerWlr*>(data)->onFailed();
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerWlr::frameBufferDone(void* data, zwlr_screencopy_frame_v1*)
{
    static_cast<ScreenCapturerWlr*>(data)->onBufferDone();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface,
                                         uint32_t version)
{
    if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0)
    {
        manager_ = static_cast<zwlr_screencopy_manager_v1*>(wl_registry_bind(
            registry, name, &zwlr_screencopy_manager_v1_interface, std::min<uint32_t>(version, 3)));
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        shm_ = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    }
    else if (strcmp(interface, wl_output_interface.name) == 0 && !output_)
    {
        // Capture the first output. Multi-output selection is a later refinement.
        output_ = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min<uint32_t>(version, 4)));
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onBuffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    // Take the first advertised shm buffer (compositors list a packed BGRA format first).
    if (frame_width_ == 0)
    {
        frame_format_ = format;
        frame_width_ = static_cast<int>(width);
        frame_height_ = static_cast<int>(height);
        frame_stride_ = static_cast<int>(stride);
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onFlags(uint32_t flags)
{
    y_invert_ = (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onReady()
{
    ready_ = true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onFailed()
{
    failed_ = true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWlr::onBufferDone()
{
    // The buffer parameters are read after a roundtrip, so nothing extra is needed here.
}

//--------------------------------------------------------------------------------------------------
const wl_registry_listener ScreenCapturerWlr::kRegistryListener =
{
    &ScreenCapturerWlr::registryGlobal, registryGlobalRemove
};

//--------------------------------------------------------------------------------------------------
const zwlr_screencopy_frame_v1_listener ScreenCapturerWlr::kFrameListener =
{
    &ScreenCapturerWlr::frameBuffer, &ScreenCapturerWlr::frameFlags, &ScreenCapturerWlr::frameReady,
    &ScreenCapturerWlr::frameFailed, frameDamage, frameLinuxDmabuf, &ScreenCapturerWlr::frameBufferDone
};
