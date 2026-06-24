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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_KMS_H
#define BASE_DESKTOP_SCREEN_CAPTURER_KMS_H

#include <QRect>

#include <memory>

#include "base/desktop/screen_capturer.h"

class Differ;
class EglDmaBuf;
class MouseCursor;

// Captures the active CRTC's scan-out framebuffer directly through DRM/KMS, below the compositor. A
// privileged process (CAP_SYS_ADMIN) can read any framebuffer via drmModeGetFB2(), export it as a
// DMA-BUF and import it with EGL/GBM. This is compositor-independent (GNOME, KDE, X11) and needs no
// portal or permission, which is what allows capturing the login screen. The agent therefore runs as
// root when this capturer is used.
class ScreenCapturerKms final : public ScreenCapturer
{
public:
    explicit ScreenCapturerKms(QObject* parent = nullptr);
    ~ScreenCapturerKms() final;

    // Returns nullptr if DRM/KMS is unavailable or the EGL import path could not be initialized.
    static ScreenCapturerKms* create(QObject* parent = nullptr);

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
    // Returns the framebuffer id currently scanned out on an active CRTC, or 0 if none. Also records
    // the captured CRTC id and the logical desktop extent across all active CRTCs.
    quint32 activeFramebufferId();
    // Finds the hardware cursor plane on the captured CRTC, returning its framebuffer id, size and
    // position (any output pointer may be null). Returns false if no cursor plane is active there.
    bool findCursorPlane(quint32* fb_id, QSize* size, QPoint* position);

    int drm_fd_ = -1;
    quint32 crtc_id_ = 0;
    std::unique_ptr<EglDmaBuf> egl_dmabuf_;
    std::unique_ptr<Differ> differ_;
    FrameQueue<Frame> queue_;

    // The hardware cursor lives on a separate KMS plane, so it is captured independently of the
    // screen framebuffer. The shape is only re-read when the cursor plane's framebuffer changes.
    std::unique_ptr<MouseCursor> mouse_cursor_;
    quint32 last_cursor_fb_id_ = 0;
    QPoint cursor_position_;

    QRect screen_rect_;
    // Logical desktop extent across all active CRTCs. The compositor maps the absolute pointer over
    // this whole area, so input must be scaled to it rather than to the captured CRTC alone.
    QRect desktop_rect_;

    Q_DISABLE_COPY_MOVE(ScreenCapturerKms)
};

#endif // BASE_DESKTOP_SCREEN_CAPTURER_KMS_H
