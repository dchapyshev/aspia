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

#include "desktop/win/font_smoothing_disabler.h"

#include <Windows.h>

namespace desktop {

FontSmoothingDisabler::FontSmoothingDisabler()
{
    BOOL enabled;
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &enabled, 0))
        is_enabled_ = !!enabled;

    if (SystemParametersInfoW(SPI_SETFONTSMOOTHING, FALSE, 0, SPIF_SENDCHANGE))
        is_changed_ = true;
}

FontSmoothingDisabler::~FontSmoothingDisabler()
{
    if (is_changed_ && is_enabled_)
        SystemParametersInfoW(SPI_SETFONTSMOOTHING, !!is_enabled_, 0, SPIF_SENDCHANGE);
}

} // namespace desktop
