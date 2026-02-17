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

#include "base/desktop/desktop_resizer_x11.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
DesktopResizerX11::DesktopResizerX11() = default;

//--------------------------------------------------------------------------------------------------
DesktopResizerX11::~DesktopResizerX11() = default;

//--------------------------------------------------------------------------------------------------
QList<QSize> DesktopResizerX11::supportedResolutions(ScreenId /* screen_id */)
{
    NOTIMPLEMENTED();
    return {};
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerX11::setResolution(ScreenId /* screen_id */, const QSize& /* resolution */)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerX11::restoreResolution(ScreenId /* screen_id */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerX11::restoreResulution()
{
    NOTIMPLEMENTED();
}

} // namespace base
