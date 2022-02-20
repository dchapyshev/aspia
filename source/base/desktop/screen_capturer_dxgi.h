//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H
#define BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H

#include "base/desktop/screen_capturer.h"
#include "base/desktop/win/dxgi_duplicator_controller.h"
#include "base/desktop/win/dxgi_frame.h"

namespace base {

class ScreenCapturerDxgi : public ScreenCapturer
{
public:
    ScreenCapturerDxgi();
    ~ScreenCapturerDxgi() override;

    // Whether the system supports DXGI based capturing.
    bool isSupported();

    // Whether current process is running in a Windows session which is supported by
    // ScreenCapturerDxgi.
    // Usually using ScreenCapturerDxgi in unsupported sessions will fail.
    // But this behavior may vary on different Windows version. So consumers can
    // always try isSupported() function.
    static bool isCurrentSessionSupported();

    // ScreenCapturer implementation.
    int screenCount() override;
    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;
    ScreenId currentScreen() const override;
    const Frame* captureFrame(Error* error) override;
    const MouseCursor* captureCursor() override;
    Point cursorPosition() override;

protected:
    // ScreenCapturer implementation.
    void reset() override;

private:
    std::shared_ptr<DxgiDuplicatorController> controller_;

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    FrameQueue<DxgiFrame> queue_;
    std::unique_ptr<DxgiCursor> cursor_;
    std::vector<std::pair<Rect, Point>> dpi_for_rect_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerDxgi);
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H
