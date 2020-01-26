//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__SCREEN_CAPTURER_DXGI_H
#define DESKTOP__SCREEN_CAPTURER_DXGI_H

#include "desktop/screen_capturer.h"
#include "desktop/screen_capture_frame_queue.h"
#include "desktop/win/dxgi_duplicator_controller.h"
#include "desktop/win/dxgi_frame.h"

namespace desktop {

class ScreenCapturerDxgi : public ScreenCapturer
{
public:
    ScreenCapturerDxgi();
    ~ScreenCapturerDxgi();

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
    std::unique_ptr<SharedFrame> captureFrame(Error* error) override;

protected:
    // ScreenCapturer implementation.
    void reset() override;

private:
    std::shared_ptr<DxgiDuplicatorController> controller_;

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    ScreenCaptureFrameQueue<DxgiFrame> queue_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerDxgi);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURER_DXGI_H
