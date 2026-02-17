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

#ifndef BASE_DESKTOP_WIN_SCREEN_CAPTURE_UTILS_H
#define BASE_DESKTOP_WIN_SCREEN_CAPTURE_UTILS_H

#include "base/desktop/screen_capturer.h"

namespace base {

class ScreenCaptureUtils
{
public:
    // Output the list of active screens into |screens|. Returns true if succeeded, or false if it
    // fails to enumerate the display devices.
    static bool screenList(ScreenCapturer::ScreenList* screens);

    // Returns true if |screen| is a valid screen. The screen device key is returned through
    // |device_key| if the screen is valid. The device key can be used in screenRect to verify the
    // screen matches the previously obtained id.
    static bool isScreenValid(ScreenCapturer::ScreenId screen, std::wstring* device_key);

    // Get the rect of the entire system in system coordinate system. I.e. the primary monitor
    // always starts from (0, 0).
    static QRect fullScreenRect();

    // Get the rect of the screen identified by |screen|, relative to the primary display's
    // top-left.
    // If the screen device key does not match |device_key|, or the screen does not exist, or any
    // error happens, an empty rect is returned.
    static QRect screenRect(ScreenCapturer::ScreenId screen, const std::wstring& device_key);

    // The number of visible display monitors on a desktop.
    static int screenCount();

private:
    Q_DISABLE_COPY(ScreenCaptureUtils)
};

} // namespace base

#endif // BASE_DESKTOP_WIN_SCREEN_CAPTURE_UTILS_H
