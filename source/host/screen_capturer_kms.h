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

#ifndef HOST_SCREEN_CAPTURER_KMS_H
#define HOST_SCREEN_CAPTURER_KMS_H

#include <QRect>
#include <QString>

#include <memory>

#include "host/screen_capturer.h"

class Differ;
class EglDmaBuf;
class MouseCursor;

typedef struct _drmModeFB2 drmModeFB2;

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

    // Returns true only if a capturer initializes AND a trial frame is captured successfully. Init alone
    // can succeed on a GPU that cannot actually export its scan-out framebuffer, so a probe capture is
    // required before committing to this backend.
    static bool isAvailable();

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
    // Reads the active scan-out once into a throwaway buffer to confirm capture works and to pick the
    // readback method (EGL, or the CPU DMA-BUF mapping used when GL readback is unavailable). Called from
    // init(), so the method is fixed before the first real frame.
    bool probeReadback();

    // Imports framebuffer |fb| into |dst| (|dst_stride| bytes per row) as packed BGRA, via EGL/GBM or,
    // once GL readback is found unavailable, a direct CPU DMA-BUF mapping. Releases the GEM handles that
    // drmModeGetFB2() opened for |fb|.
    bool importFb(drmModeFB2* fb, quint8* dst, int dst_stride);

    // Returns the framebuffer id currently scanned out on an active CRTC, or 0 if none. Records the
    // captured CRTC id and the active-CRTC count (a change re-triggers input-geometry detection).
    quint32 activeFramebufferId();
    // Reads the compositor's logical monitor layout over Wayland and computes the screen size and the
    // captured monitor's offset to report to the input injector (folding in the monitor position and
    // fractional scale). Falls back to the captured size alone when the layout is unavailable.
    void updateInputGeometry(const QSize& captured);

    // Returns the connector name of the captured CRTC (e.g. "eDP-1"), used to match it to the right
    // compositor output. Empty if it cannot be resolved.
    QString capturedConnectorName();

    // Finds the hardware cursor plane on the captured CRTC, returning its framebuffer id, size and
    // position (any output pointer may be null). Returns false if no cursor plane is active there.
    bool findCursorPlane(quint32* fb_id, QSize* size, QPoint* position);

    int drm_fd_ = -1;
    quint32 crtc_id_ = 0;
    // CRTC the client selected for capture (0 = none yet; the first active CRTC is captured then).
    quint32 selected_crtc_id_ = 0;
    int active_crtc_count_ = 0;
    std::unique_ptr<EglDmaBuf> egl_dmabuf_;
    // Set once (on the EGL->CPU fallback) when GL readback is unavailable, so later frames go straight to
    // the CPU DMA-BUF mapping instead of retrying the failing EGL import every time.
    bool prefer_cpu_readback_ = false;
    std::unique_ptr<Differ> differ_;
    FrameQueue<Frame> queue_;

    // The hardware cursor lives on a separate KMS plane, so it is captured independently of the
    // screen framebuffer. The shape is only re-read when the cursor plane's framebuffer changes.
    std::unique_ptr<MouseCursor> mouse_cursor_;
    quint32 last_cursor_fb_id_ = 0;
    QPoint cursor_position_;

    QRect screen_rect_;
    // Input-mapping geometry from the compositor's logical layout: the compositor maps the absolute
    // pointer over the whole logical desktop, so the injector is given this (scaled) desktop size and
    // the captured monitor's offset within it, not the captured CRTC alone.
    QRect desktop_rect_;
    QPoint capture_offset_;
    bool input_geometry_valid_ = false;
    int input_geometry_attempts_ = 0;

    Q_DISABLE_COPY_MOVE(ScreenCapturerKms)
};

#endif // HOST_SCREEN_CAPTURER_KMS_H
