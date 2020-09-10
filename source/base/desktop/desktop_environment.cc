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

#include "base/desktop/desktop_environment.h"

namespace base {

DesktopEnvironment::DesktopEnvironment()
{
    applyNewSettings();
}

DesktopEnvironment::~DesktopEnvironment()
{
    revertAll();
}

void DesktopEnvironment::setWallpaper(bool enable)
{
    if (wallpaper_ == enable)
        return;

    wallpaper_ = enable;

    revertAll();
    applyNewSettings();
}

void DesktopEnvironment::setFontSmoothing(bool enable)
{
    if (font_smoothing_ == enable)
        return;

    font_smoothing_ = enable;

    revertAll();
    applyNewSettings();
}

void DesktopEnvironment::setEffects(bool enable)
{
    if (effects_ == enable)
        return;

    effects_ = enable;

    revertAll();
    applyNewSettings();
}

void DesktopEnvironment::applyNewSettings()
{
    if (!wallpaper_)
        disableWallpaper();

    if (!font_smoothing_)
        disableFontSmoothing();

    if (!effects_)
        disableEffects();
}

} // namespace base
