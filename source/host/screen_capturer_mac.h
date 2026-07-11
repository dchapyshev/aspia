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

#ifndef HOST_SCREEN_CAPTURER_MAC_H
#define HOST_SCREEN_CAPTURER_MAC_H

#include <QDeadlineTimer>
#include <QRect>

#include <memory>

#include "host/screen_capturer.h"

// Holds the ScreenCaptureKit/Objective-C state. Defined in the .mm so this header stays pure C++
// and can be included from ordinary C++ translation units (e.g. desktop_agent.cc).
struct ScreenCapturerMacImpl;

class ScreenCapturerMac final : public ScreenCapturer
{
public:
    explicit ScreenCapturerMac(QObject* parent = nullptr);
    ~ScreenCapturerMac() final;

    static ScreenCapturerMac* create(QObject* parent = nullptr);

    // Blocks until the WindowServer reports at least one online display, or the timeout expires. A
    // desktop agent launched at boot (login window) can start before the WindowServer is ready; the
    // first CoreGraphics call then binds a dead connection that never sees a display for the life of
    // the process. The agent calls this once at startup and, on failure, exits so launchd restarts it
    // against a ready WindowServer. Returns true if a display appeared.
    static bool waitForDisplays();

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;
    void resetCursorCache() final;
    const QRect& desktopRect() const final;
    const QRect& currentScreenRect() const final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

private:
    // Refreshes the cached display list and the desktop rectangle from the current shareable content.
    bool updateDisplays();
    // Reacts to a display reconfiguration (resolution, position, plug/unplug): refreshes the display
    // list and restarts the stream if the captured display changed. Returns false when the captured
    // display is gone and the capturer has to be recreated.
    bool handleDisplayChange();
    // (Re)starts the SCStream capturing |current_screen_id_|. Both the start and the stop are
    // asynchronous and never block on the WindowServer: captureFrame() reports a temporary error
    // while the start is pending and a permanent one when it fails.
    bool startStream();
    void stopStream();

    // Rect of the whole virtual desktop (all displays), in global display points.
    QRect desktop_rect_;
    // Rect of the display currently being captured.
    QRect current_screen_rect_;

    ScreenId current_screen_id_ = kInvalidScreenId;

    // Deadline for the asynchronous stream start; captureFrame() converts an expired pending start
    // into a permanent error so the agent recreates the capturer. Only touched on the capture (Qt)
    // thread.
    QDeadlineTimer start_deadline_;

    // The frame handed out to the caller. captureFrame() copies the regions accumulated in the
    // delegate's back frame into it. Only touched on the capture (Qt) thread.
    std::unique_ptr<Frame> front_frame_;

    // The last cursor shape handed out; captureCursor() returns a cursor only when the shape
    // differs from this one.
    std::unique_ptr<MouseCursor> mouse_cursor_;

    std::unique_ptr<ScreenCapturerMacImpl> impl_;

    Q_DISABLE_COPY(ScreenCapturerMac)
};

#endif // HOST_SCREEN_CAPTURER_MAC_H
