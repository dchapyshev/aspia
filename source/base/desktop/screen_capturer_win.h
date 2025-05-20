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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_WIN_H
#define BASE_DESKTOP_SCREEN_CAPTURER_WIN_H

#include "base/desktop/screen_capturer.h"
#include "base/win/scoped_thread_desktop.h"

namespace base {

class ScreenCapturerWin : public ScreenCapturer
{
    Q_OBJECT

public:
    ScreenCapturerWin(Type type, QObject* parent);
    ~ScreenCapturerWin();

    static ScreenCapturer* create(Type preferred_type, Error last_error);

    void switchToInputDesktop() final;

private:
    void checkScreenType(const wchar_t* desktop_name);

    ScopedThreadDesktop desktop_;
    ScreenType last_screen_type_ = ScreenType::UNKNOWN;
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_WIN_H
