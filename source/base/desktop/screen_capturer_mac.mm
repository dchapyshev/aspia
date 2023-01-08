//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/screen_capturer_mac.h"

#include "base/logging.h"

namespace base {

ScreenCapturerMac::ScreenCapturerMac()
    : ScreenCapturer(ScreenCapturer::Type::MACOSX)
{
    // Nothing
}

ScreenCapturerMac::~ScreenCapturerMac() = default;

int ScreenCapturerMac::screenCount()
{
    NOTIMPLEMENTED();
    return 0;
}

bool ScreenCapturerMac::screenList(ScreenList* /* screens */)
{
    NOTIMPLEMENTED();
    return false;
}

bool ScreenCapturerMac::selectScreen(ScreenId /* screen_id */)
{
    NOTIMPLEMENTED();
    return false;
}

ScreenCapturer::ScreenId ScreenCapturerMac::currentScreen() const
{
    NOTIMPLEMENTED();
    return kInvalidScreenId;
}

const Frame* ScreenCapturerMac::captureFrame(Error* error)
{
    DCHECK(error);
    NOTIMPLEMENTED();
    *error = Error::PERMANENT;
    return nullptr;
}

const MouseCursor* ScreenCapturerMac::captureCursor()
{
    NOTIMPLEMENTED();
    return nullptr;
}

Point ScreenCapturerMac::cursorPosition()
{
    NOTIMPLEMENTED();
    return Point();
}

void ScreenCapturerMac::reset()
{
    NOTIMPLEMENTED();
}

} // namespace base
