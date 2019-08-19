//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__SCREEN_CAPTURER_H
#define DESKTOP__SCREEN_CAPTURER_H

#include "desktop/desktop_frame.h"

#include <string>
#include <vector>

namespace desktop {

class ScreenCapturer
{
public:
    virtual ~ScreenCapturer() = default;

    enum class Error { SUCCEEDED, PERMANENT, TEMPORARY };

    using ScreenId = intptr_t;

    struct Screen
    {
        ScreenId id;
        std::string title;
    };

    using ScreenList = std::vector<Screen>;

    static const ScreenId kFullDesktopScreenId = -1;
    static const ScreenId kInvalidScreenId = -2;

    virtual int screenCount() = 0;
    virtual bool screenList(ScreenList* screens) = 0;
    virtual bool selectScreen(ScreenId screen_id) = 0;
    virtual const Frame* captureFrame(Error* error) = 0;

protected:
    friend class ScreenCapturerWrapper;
    virtual void reset() = 0;
};

} // namespace desktop

#endif // DESKTOP__CAPTURER_H
