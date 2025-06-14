//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_MAC_H
#define BASE_DESKTOP_SCREEN_CAPTURER_MAC_H

#include "base/desktop/screen_capturer.h"

#include "base/desktop/mouse_cursor.h"
#include "base/desktop/screen_capturer_helper.h"
#include "base/desktop/shared_frame.h"
#include "base/desktop/mac/frame_provider.h"
#include "base/desktop/mac/desktop_configuration.h"
#include "base/desktop/mac/desktop_configuration_monitor.h"
#include "base/threading/thread_checker.h"

#include <vector>

#include <AppKit/NSImage.h>

namespace base {

class ScreenCapturerMac final : public ScreenCapturer
{
public:
    ScreenCapturerMac();
    ~ScreenCapturerMac() final;

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    Point cursorPosition() final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

private:
    // Returns false if the selected screen is no longer valid.
    bool cgBlit(const Frame& frame, const Region& region);

    // Called when the screen configuration is changed.
    void screenConfigurationChanged();

    bool registerRefreshAndMoveHandlers();
    void unregisterRefreshAndMoveHandlers();

    void screenRefresh(CGDirectDisplayID display_id, CGRectCount count, const CGRect* rect_array,
                       const Point& display_origin, IOSurfaceRef io_surface);
    void releaseBuffers();

    std::unique_ptr<Frame> createFrame();

    const bool detect_updated_region_;

    // Queue of the frames buffers.
    FrameQueue<SharedFrame> queue_;

    // Current display configuration.
    MacDesktopConfiguration desktop_config_;

    // Currently selected display, or 0 if the full desktop is selected. On OS X
    // 10.6 and before, this is always 0.
    CGDirectDisplayID current_display_ = 0;

    // The physical pixel bounds of the current screen.
    Rect screen_pixel_bounds_;

    // The dip to physical pixel scale of the current screen.
    float dip_to_pixel_scale_ = 1.0f;

    // A thread-safe list of invalid rectangles, and the size of the most
    // recently captured screen.
    ScreenCapturerHelper helper_;

    // Contains an invalid region from the previous capture.
    Region last_invalid_region_;

    // Monitoring display reconfiguration.
    std::shared_ptr<DesktopConfigurationMonitor> desktop_config_monitor_;

    // List of streams, one per screen.
    std::vector<CGDisplayStreamRef> display_streams_;

    // Container holding latest state of the snapshot per displays.
    FrameProvider desktop_frame_provider_;

    ScreenId current_screen_id_ = kFullDesktopScreenId;

    __strong NSImage* last_ns_cursor_ = nullptr;
    std::unique_ptr<MouseCursor> last_cursor_;

    THREAD_CHECKER(thread_checker_);

    Q_DISABLE_COPY(ScreenCapturerMac)
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_MAC_H
