//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

namespace base {

DesktopEnvironment::DesktopEnvironment()
{
    LOG(LS_INFO) << "Ctor";
}

DesktopEnvironment::~DesktopEnvironment()
{
    LOG(LS_INFO) << "Dtor";
}

void DesktopEnvironment::setWallpaper(bool enable)
{
    static const bool kDefaultValue = true;

    bool has_changes = false;
    if (wallpaper_.has_value())
    {
        has_changes = true;
    }
    else
    {
        if (enable != kDefaultValue)
            has_changes = true;
    }

    if (has_changes)
    {
        wallpaper_.emplace(enable);

        revertAll();
        applyNewSettings();
    }
}

void DesktopEnvironment::setFontSmoothing(bool enable)
{
    static const bool kDefaultValue = true;

    bool has_changes = false;
    if (font_smoothing_.has_value())
    {
        has_changes = true;
    }
    else
    {
        if (enable != kDefaultValue)
            has_changes = true;
    }

    if (has_changes)
    {
        font_smoothing_.emplace(enable);

        revertAll();
        applyNewSettings();
    }
}

void DesktopEnvironment::setEffects(bool enable)
{
    static const bool kDefaultValue = true;

    bool has_changes = false;
    if (effects_.has_value())
    {
        has_changes = true;
    }
    else
    {
        if (enable != kDefaultValue)
            has_changes = true;
    }

    if (has_changes)
    {
        effects_.emplace(enable);

        revertAll();
        applyNewSettings();
    }
}

void DesktopEnvironment::onDesktopChanged()
{
    applyNewSettings();
}

void DesktopEnvironment::applyNewSettings()
{
    if (wallpaper_.has_value() && !*wallpaper_)
        disableWallpaper();

    if (font_smoothing_.has_value() && !*font_smoothing_)
        disableFontSmoothing();

    if (effects_.has_value() && !*effects_)
        disableEffects();
}

} // namespace base
