//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_H_
#define ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_H_

#include <QString>
#include <vector>

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class ScreenCapturer
{
public:
    virtual ~ScreenCapturer() = default;

    using ScreenId = intptr_t;

    struct Screen
    {
        ScreenId id;
        QString title;
    };

    using ScreenList = std::vector<Screen>;

    static const ScreenId kFullDesktopScreenId = -1;
    static const ScreenId kInvalidScreenId = -2;

    virtual int screenCount() = 0;
    virtual bool screenList(ScreenList* screens) = 0;
    virtual bool selectScreen(ScreenId screen_id) = 0;
    virtual const DesktopFrame* captureFrame() = 0;
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__CAPTURER_H_
