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

#include <QByteArray>
#include <QRect>
#include <QString>

#include <memory>
#include <vector>

#include "host/screen_capturer.h"

class Differ;
class EglDmaBuf;
class MouseCursor;

typedef struct _drmModeFB2 drmModeFB2;

// Captures the active CRTC's scan-out framebuffer directly through DRM/KMS, below the compositor. A
// privileged process (CAP_SYS_ADMIN) can read any framebuffer via drmModeGetFB2(), export it as a
// DMA-BUF and import it with EGL/GBM. This is compositor-independent (GNOME, KDE, X11) and needs no
// portal or permission, which is what allows capturing the login screen. The agent therefore runs as
// root when this capturer is used. Monitors on every DRM card are offered as screens, so a laptop
// with two GPUs driving one monitor each shows both.
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
    enum class Readback { UNKNOWN, EGL, DMABUF_CPU, DUMB_CPU, UNUSABLE };

    // One DRM card with CRTCs. The readback method is driver-specific, so it is probed per card, and
    // the EGL import runs on the render node of the same card.
    struct Card
    {
        QByteArray path;
        int fd = -1;
        Readback readback = Readback::UNKNOWN;
        std::unique_ptr<EglDmaBuf> egl;
    };

    bool init();

    // Makes |index| the captured card, probing its readback method on first use. Returns false if
    // the card cannot be read, and the captured card is then unchanged.
    bool switchCard(int index);

    // Picks the readback method of the captured card by importing its active scan-out once per
    // candidate (EGL, CPU DMA-BUF mapping, CPU dumb-buffer mapping) and keeping the first that works.
    // Called before the first real frame from the card; also confirms capture is possible at all.
    bool probeReadback();

    // Imports framebuffer |fb| into |dst| (|dst_stride| bytes per row) as packed BGRA using the readback
    // method already chosen by probeReadback(). Releases the GEM handles drmModeGetFB2() opened for |fb|.
    bool importFb(drmModeFB2* fb, quint8* dst, int dst_stride);

    // Returns the framebuffer id currently scanned out on an active CRTC, or 0 if none, switching to
    // the card that has one when the captured card has none. Records the captured CRTC id and the
    // active-CRTC count (a change re-triggers input-geometry detection).
    quint32 activeFramebufferId();
    // Reads the compositor's logical monitor layout over Wayland and computes the screen size and the
    // captured monitor's offset to report to the input injector (folding in the monitor position and
    // fractional scale). Falls back to the captured size alone when the layout is unavailable.
    void updateInputGeometry(const QSize& captured);

    // Returns the connector name of the captured CRTC (e.g. "eDP-1"), used to match it to the right
    // compositor output. Empty if it cannot be resolved.
    QString capturedConnectorName();

    // Finds the hardware cursor plane on the captured CRTC, returning its framebuffer id, size,
    // position and hotspot (any output pointer may be null). Returns false if no cursor plane is active
    // there.
    bool findCursorPlane(quint32* fb_id, QSize* size, QPoint* position, QPoint* hotspot);

    Card& card() { return cards_[card_index_]; }

    std::vector<Card> cards_;
    int card_index_ = 0;
    quint32 crtc_id_ = 0;
    // Card and CRTC the client selected for capture (-1 and 0 = none yet; the first active CRTC is captured then).
    int selected_card_ = -1;
    quint32 selected_crtc_id_ = 0;
    int active_crtc_count_ = 0;
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
