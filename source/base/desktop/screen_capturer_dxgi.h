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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H
#define BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H

#include "base/desktop/screen_capturer_win.h"
#include "base/desktop/win/dxgi_duplicator_controller.h"
#include "base/desktop/win/dxgi_frame.h"

namespace base {

class ScreenCapturerDxgi final : public ScreenCapturerWin
{
    Q_OBJECT

public:
    explicit ScreenCapturerDxgi(QObject* parent = nullptr);
    ~ScreenCapturerDxgi() final;

    // Whether the system supports DXGI based capturing.
    bool isSupported();

    // Whether current process is running in a Windows session which is supported by
    // ScreenCapturerDxgi.
    // Usually using ScreenCapturerDxgi in unsupported sessions will fail.
    // But this behavior may vary on different Windows version. So consumers can
    // always try isSupported() function.
    static bool isCurrentSessionSupported();

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
    base::local_shared_ptr<DxgiDuplicatorController> controller_;

    int current_screen_index_ = -1;
    ScreenId current_screen_id_ = kFullDesktopScreenId;
    FrameQueue<DxgiFrame> queue_;
    std::unique_ptr<DxgiCursor> cursor_;
    std::vector<std::pair<Rect, Point>> dpi_for_rect_;
    int temporary_error_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerDxgi);
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_DXGI_H
