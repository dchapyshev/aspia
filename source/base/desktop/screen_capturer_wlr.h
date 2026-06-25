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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_WLR_H
#define BASE_DESKTOP_SCREEN_CAPTURER_WLR_H

#include <QRect>

#include <cstdint>
#include <memory>

#include <sys/types.h>

#include "base/desktop/frame.h"
#include "base/desktop/screen_capturer.h"

struct wl_buffer;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_registry_listener;
struct wl_shm;
struct zwlr_screencopy_frame_v1;
struct zwlr_screencopy_frame_v1_listener;
struct zwlr_screencopy_manager_v1;

class Differ;

// Captures a wlroots-based compositor (sway, labwc, Wayfire, Raspberry Pi OS, ...) through the
// zwlr_screencopy_unstable_v1 Wayland protocol. Unlike Mutter/KWin (D-Bus on the session bus), the
// Wayland socket has no per-uid authentication, so the root host connects to it directly - no
// credential drop. Each frame is copied into a reused wl_shm buffer; one output is captured. Input is
// injected separately via uinput.
class ScreenCapturerWlr final : public ScreenCapturer
{
    Q_OBJECT

public:
    explicit ScreenCapturerWlr(uid_t session_uid, QObject* parent = nullptr);
    ~ScreenCapturerWlr() final;

    // Returns true if zwlr_screencopy_manager_v1 is advertised on |session_uid|'s Wayland socket.
    static bool isAvailable(uid_t session_uid);

    // Returns nullptr if the protocol is unavailable or the first capture failed.
    static ScreenCapturerWlr* create(uid_t session_uid, QObject* parent = nullptr);

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;
    const QRect& desktopRect() const final;
    const QRect& currentScreenRect() const final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

private:
    bool init();
    bool capture();
    bool ensureBuffer(uint32_t format, int width, int height, int stride);
    void releaseBuffer();
    void disconnect();

    // wl_registry / zwlr_screencopy_frame_v1 C callbacks. Static so the on*() handlers below can stay
    // private; each forwards |data| to the matching handler on the instance.
    static void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface,
                               uint32_t version);
    static void frameBuffer(void* data, zwlr_screencopy_frame_v1* frame, uint32_t format,
                            uint32_t width, uint32_t height, uint32_t stride);
    static void frameFlags(void* data, zwlr_screencopy_frame_v1* frame, uint32_t flags);
    static void frameReady(void* data, zwlr_screencopy_frame_v1* frame, uint32_t sec_hi,
                           uint32_t sec_lo, uint32_t nsec);
    static void frameFailed(void* data, zwlr_screencopy_frame_v1* frame);
    static void frameBufferDone(void* data, zwlr_screencopy_frame_v1* frame);

    void onRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface,
                          uint32_t version);
    void onBuffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    void onFlags(uint32_t flags);
    void onReady();
    void onFailed();
    void onBufferDone();

    static const wl_registry_listener kRegistryListener;
    static const zwlr_screencopy_frame_v1_listener kFrameListener;

    const uid_t session_uid_;

    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    zwlr_screencopy_manager_v1* manager_ = nullptr;
    wl_shm* shm_ = nullptr;
    wl_output* output_ = nullptr;

    // Reused shm capture buffer.
    int shm_fd_ = -1;
    void* shm_data_ = nullptr;
    size_t shm_size_ = 0;
    wl_buffer* buffer_ = nullptr;
    uint32_t buffer_format_ = 0;
    int buffer_width_ = 0;
    int buffer_height_ = 0;
    int buffer_stride_ = 0;

    // Per-frame state set by the listener.
    uint32_t frame_format_ = 0;
    int frame_width_ = 0;
    int frame_height_ = 0;
    int frame_stride_ = 0;
    bool y_invert_ = false;
    bool ready_ = false;
    bool failed_ = false;

    FrameQueue<Frame> queue_;
    std::unique_ptr<Differ> differ_;
    QRect screen_rect_;

    Q_DISABLE_COPY_MOVE(ScreenCapturerWlr)
};

#endif // BASE_DESKTOP_SCREEN_CAPTURER_WLR_H
