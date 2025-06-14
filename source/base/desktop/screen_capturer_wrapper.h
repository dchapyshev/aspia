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

#include <QPointer>

#include "base/desktop/screen_capturer.h"

namespace base {

class DesktopEnvironment;
class DesktopResizer;
class MouseCursor;
class PowerSaveBlocker;

class ScreenCapturerWrapper final : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturerWrapper(ScreenCapturer::Type preferred_type, QObject* parent = nullptr);
    ~ScreenCapturerWrapper();

    void selectScreen(ScreenCapturer::ScreenId screen_id, const Size& resolution);
    ScreenCapturer::Error captureFrame(const Frame** frame, const MouseCursor** mouse_cursor);
    void setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory);
    void enableWallpaper(bool enable);
    void enableEffects(bool enable);
    void enableFontSmoothing(bool enable);
    void enableCursorPosition(bool enable);

signals:
    void sig_screenListChanged(
        const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current);
    void sig_cursorPositionChanged(const base::Point& position);
    void sig_screenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name);

private:
    ScreenCapturer::ScreenId defaultScreen();
    void selectCapturer(ScreenCapturer::Error last_error);

    SharedMemoryFactory* shared_memory_factory_ = nullptr;
    ScreenCapturer::Type preferred_type_;

    int screen_count_ = 0;
    ScreenCapturer::ScreenId last_screen_id_ = ScreenCapturer::kInvalidScreenId;
    Point last_cursor_pos_;
    bool enable_cursor_position_ = false;

    std::unique_ptr<PowerSaveBlocker> power_save_blocker_;
    DesktopEnvironment* environment_ = nullptr;
    std::unique_ptr<DesktopResizer> resizer_;
    QPointer<ScreenCapturer> screen_capturer_ = nullptr;

    Q_DISABLE_COPY(ScreenCapturerWrapper)
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_WRAPPER_H
