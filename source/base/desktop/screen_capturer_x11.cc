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

#include "base/desktop/screen_capturer_x11.h"

#include "base/logging.h"

namespace base {

ScreenCapturerX11::ScreenCapturerX11()
    : ScreenCapturer(ScreenCapturer::Type::LINUX_X11)
{
    // Nothing
}

ScreenCapturerX11::~ScreenCapturerX11() = default;

int ScreenCapturerX11::screenCount()
{
    NOTIMPLEMENTED();
    return 0;
}

bool ScreenCapturerX11::screenList(ScreenList* /* screens */)
{
    NOTIMPLEMENTED();
    return false;
}

bool ScreenCapturerX11::selectScreen(ScreenId /* screen_id */)
{
    NOTIMPLEMENTED();
    return false;
}

ScreenCapturer::ScreenId ScreenCapturerX11::currentScreen() const
{
    NOTIMPLEMENTED();
    return kInvalidScreenId;
}

const Frame* ScreenCapturerX11::captureFrame(Error* error)
{
    DCHECK(error);
    NOTIMPLEMENTED();
    *error = Error::PERMANENT;
    return nullptr;
}

const MouseCursor* ScreenCapturerX11::captureCursor()
{
    NOTIMPLEMENTED();
    return nullptr;
}

Point ScreenCapturerX11::cursorPosition()
{
    NOTIMPLEMENTED();
    return Point();
}

void ScreenCapturerX11::reset()
{
    NOTIMPLEMENTED();
}

} // namespace base
