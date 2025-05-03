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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_WRAPPER_H
#define BASE_DESKTOP_SCREEN_CAPTURER_WRAPPER_H

#include "base/desktop/screen_capturer.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_thread_desktop.h"
#elif defined(OS_LINUX)
#endif

namespace base {

class DesktopEnvironment;
class DesktopResizer;
class MouseCursor;
class PowerSaveBlocker;

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
        virtual void onScreenCaptureError(ScreenCapturer::Error error) = 0;
        virtual void onCursorPositionChanged(const Point& position) = 0;
        virtual void onScreenTypeChanged(ScreenCapturer::ScreenType type, const QString& name) = 0;
    };

    ScreenCapturerWrapper(ScreenCapturer::Type preferred_type, Delegate* delegate);
    ~ScreenCapturerWrapper();

    void selectScreen(ScreenCapturer::ScreenId screen_id, const Size& resolution);
    void captureFrame();
    void setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory);
    void enableWallpaper(bool enable);
    void enableEffects(bool enable);
    void enableFontSmoothing(bool enable);
    void enableCursorPosition(bool enable);

private:
    ScreenCapturer::ScreenId defaultScreen();
    void selectCapturer(ScreenCapturer::Error last_error);
    void switchToInputDesktop();
    void checkScreenType();

    SharedMemoryFactory* shared_memory_factory_ = nullptr;
    ScreenCapturer::Type preferred_type_;
    Delegate* delegate_;

#if defined(OS_WIN)
    ScopedThreadDesktop desktop_;
#endif // defined(OS_WIN)

    int screen_count_ = 0;
    ScreenCapturer::ScreenId last_screen_id_ = ScreenCapturer::kInvalidScreenId;
    ScreenCapturer::ScreenType last_screen_type_ = ScreenCapturer::ScreenType::UNKNOWN;
    Point last_cursor_pos_;
    bool enable_cursor_position_ = false;
    quint32 capture_counter_ = 0;

    std::unique_ptr<PowerSaveBlocker> power_save_blocker_;
    std::unique_ptr<DesktopEnvironment> environment_;
    std::unique_ptr<DesktopResizer> resizer_;
    std::unique_ptr<ScreenCapturer> screen_capturer_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWrapper);
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_WRAPPER_H
