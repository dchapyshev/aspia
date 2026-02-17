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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_X11_H
#define BASE_DESKTOP_SCREEN_CAPTURER_X11_H

#include "base/desktop/screen_capturer.h"
#include "base/desktop/screen_capturer_helper.h"
#include "base/desktop/frame.h"
#include "base/desktop/x11/x_atom_cache.h"
#include "base/desktop/x11/x_server_pixel_buffer.h"
#include "base/desktop/x11/shared_x_display.h"

#include "base/x11/x11_headers.h"

namespace base {

class ScreenCapturerX11 final
    : public ScreenCapturer,
      public SharedXDisplay::XEventHandler
{
public:
    explicit ScreenCapturerX11(QObject* parent = nullptr);
    ~ScreenCapturerX11() final;

    static ScreenCapturerX11* create(QObject* parent = nullptr);

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

    bool init();

private:
    Display* display() { return display_->display(); }

    // SharedXDisplay::XEventHandler implementation.
    bool handleXEvent(const XEvent& event) final;

    void initXDamage();
    void initXrandr();
    void updateMonitors();

    // Called when the screen configuration is changed.
    void screenConfigurationChanged();

    // Synchronize the current buffer with |last_buffer_|, by copying pixels from the area of
    // |last_invalid_rects|.
    // Note this only works on the assumption that kNumBuffers == 2, as |last_invalid_rects| holds
    // the differences from the previous buffer and the one prior to that (which will then be the
    // current buffer).
    void synchronizeFrame();

    void deinitXlib();

    Frame* captureFrameImpl();

    std::shared_ptr<SharedXDisplay> display_;

    // X11 graphics context.
    GC gc_ = nullptr;
    Window root_window_ = BadValue;

    // XRandR 1.5 monitors.
    bool use_randr_ = false;
    int randr_event_base_ = 0;
    XRRMonitorInfo* monitors_ = nullptr;
    int num_monitors_ = 0;
    QRect selected_monitor_rect_;

    // selected_monitor_name_ will be changed to kFullDesktopScreenId
    // by a call to SelectSource() at the end of Init() because
    // selected_monitor_rect_ should be updated as well.
    // Setting it to kFullDesktopScreenId here might be misleading.
    Atom selected_monitor_name_ = 0;

    typedef XRRMonitorInfo* (*get_monitors_func)(Display*, Window, X11_Bool, int*);
    typedef void (*free_monitors_func)(XRRMonitorInfo*);
    get_monitors_func get_monitors_ = nullptr;
    free_monitors_func free_monitors_ = nullptr;

    // XFixes.
    bool has_xfixes_ = false;
    int xfixes_event_base_ = -1;
    int xfixes_error_base_ = -1;

    // XDamage information.
    bool use_damage_ = false;
    Damage damage_handle_ = 0;
    int damage_event_base_ = -1;
    int damage_error_base_ = -1;
    XserverRegion damage_region_ = 0;

    // Access to the X Server's pixel buffer.
    XServerPixelBuffer x_server_pixel_buffer_;

    // A thread-safe list of invalid rectangles, and the size of the most
    // recently captured screen.
    ScreenCapturerHelper helper_;

    // Queue of the frames buffers.
    FrameQueue<Frame> queue_;

    // Invalid region from the previous capture. This is used to synchronize the
    // current with the last buffer used.
    QRegion last_invalid_region_;

    std::unique_ptr<XAtomCache> atom_cache_;
    std::unique_ptr<MouseCursor> mouse_cursor_;

    Q_DISABLE_COPY(ScreenCapturerX11)
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_X11_H
