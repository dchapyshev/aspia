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

#ifndef HOST_SCREEN_CAPTURER_KWIN_H
#define HOST_SCREEN_CAPTURER_KWIN_H

#include <QByteArray>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QRect>
#include <QString>

#include <memory>

#include <sys/types.h>

#include "base/desktop/frame.h"
#include "host/screen_capturer.h"
#include "host/linux/wayland_output_layout.h"

class Differ;
class EglDmaBuf;
class MouseCursor;
class QDBusInterface;

// Captures the KDE Plasma desktop through KWin's private org.kde.KWin.ScreenShot2 D-Bus interface.
// CaptureWorkspace renders the whole workspace into a pipe as raw pixels; the metadata (size, stride,
// format) comes back in the method reply. There is no permission dialog: KWin authorizes the caller by
// matching its executable path to a .desktop that declares X-KDE-DBUS-Restricted-Interfaces. The host
// runs as root, so the session bus connection is made as the session user (see SessionDBus). This is a
// per-frame screenshot poll (KWin exposes no screen-cast stream to clients), used when Mutter is not
// present. Input is injected separately via uinput.
class ScreenCapturerKwin final : public ScreenCapturer
{
    Q_OBJECT

public:
    explicit ScreenCapturerKwin(uid_t session_uid, QObject* parent = nullptr);
    ~ScreenCapturerKwin() final;

    // Returns true if org.kde.KWin.ScreenShot2 is reachable on |session_uid|'s session bus.
    static bool isAvailable(uid_t session_uid);

    // Returns nullptr if the interface is unavailable or the first capture failed.
    static ScreenCapturerKwin* create(uid_t session_uid, QObject* parent = nullptr);

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
    // Captures the selected output (or the whole workspace when none is selected) into the current queue
    // frame, allocating it to match. Returns false on any D-Bus or pipe error.
    bool capture();
    // Re-reads the compositor's output layout into |outputs_| over Wayland (used for the screen list and
    // input geometry); keeps the previous layout if the query fails.
    void refreshOutputs();
    // Opens the DRM device and EGL importer used to read the hardware cursor. Best-effort: on failure
    // the screen is still captured, just without a cursor.
    void initCursorCapture();
    // Finds the active hardware cursor plane (any CRTC), returning its framebuffer id, size, position,
    // hotspot (from the plane's HOTSPOT_X/Y properties) and the physical width of its CRTC (used to
    // derive the output scale). Returns false if no cursor plane is active.
    bool findCursorPlane(quint32* fb_id, QSize* size, QPoint* position, QPoint* hotspot,
                         int* crtc_width);

    const uid_t session_uid_;
    // Connection to the session user's bus, authenticated as that user (the host runs as root).
    QString connection_name_;
    QDBusConnection bus_;
    QDBusInterface* screenshot_ = nullptr;

    FrameQueue<Frame> queue_;
    std::unique_ptr<Differ> differ_;
    // Rect of the captured frame (the selected output, or the whole workspace).
    QRect screen_rect_;
    // Whole-workspace bounding box in logical coordinates, reported to the input injector so the absolute
    // pointer is mapped over the entire desktop even when a single output is captured.
    QRect desktop_rect_;

    // Output the client selected for capture by connector name (empty = whole workspace). The cached
    // compositor output layout backs the screen list and is refreshed on a throttle in screenCount().
    QString selected_output_;
    QList<WaylandOutputLayout::Output> outputs_;
    QElapsedTimer outputs_age_;
    // Reused staging buffer for the raw pixels read from the pipe (KWin's stride may differ from the
    // frame's, so a straight copy into the frame is not possible).
    QByteArray read_buffer_;

    // Hardware cursor read straight from the DRM cursor plane (ScreenShot2 exposes no separate cursor;
    // a root process can read the plane regardless of the compositor). The shape is only re-read when
    // the cursor plane's framebuffer changes.
    int drm_fd_ = -1;
    std::unique_ptr<EglDmaBuf> egl_dmabuf_;
    std::unique_ptr<MouseCursor> mouse_cursor_;
    quint32 last_cursor_fb_id_ = 0;
    QPoint cursor_position_;

    Q_DISABLE_COPY_MOVE(ScreenCapturerKwin)
};

#endif // HOST_SCREEN_CAPTURER_KWIN_H
