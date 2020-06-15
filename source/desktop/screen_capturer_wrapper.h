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

#ifndef DESKTOP__SCREEN_CAPTURER_WRAPPER_H
#define DESKTOP__SCREEN_CAPTURER_WRAPPER_H

#include "base/threading/thread_checker.h"
#include "base/win/scoped_thread_desktop.h"
#include "desktop/screen_capturer.h"

namespace desktop {

class CursorCapturer;
class DesktopEnvironment;
class MouseCursor;

class ScreenCapturerWrapper
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onScreenListChanged(
            const ScreenCapturer::ScreenList& list, ScreenCapturer::ScreenId current) = 0;
        virtual void onScreenCaptured(const Frame* frame, const MouseCursor* mouse_cursor) = 0;
    };

    explicit ScreenCapturerWrapper(Delegate* delegate);
    ~ScreenCapturerWrapper();

    void selectScreen(ScreenCapturer::ScreenId screen_id);
    void captureFrame();
    void setSharedMemoryFactory(ipc::SharedMemoryFactory* shared_memory_factory);
    void enableWallpaper(bool enable);
    void enableEffects(bool enable);
    void enableFontSmoothing(bool enable);

private:
    void selectCapturer();
    void switchToInputDesktop();

    Delegate* delegate_;

    base::ScopedThreadDesktop desktop_;
    int screen_count_ = 0;

    std::unique_ptr<DesktopEnvironment> desktop_environment_;
    std::unique_ptr<ScreenCapturer> screen_capturer_;
    std::unique_ptr<CursorCapturer> cursor_capturer_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWrapper);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURER_WRAPPER_H
