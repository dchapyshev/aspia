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

#ifndef DESKTOP__WIN__DESKTOP_ENVIRONMENT_H
#define DESKTOP__WIN__DESKTOP_ENVIRONMENT_H

#include "base/macros_magic.h"

namespace desktop {

class DesktopEnvironment
{
public:
    DesktopEnvironment();
    ~DesktopEnvironment();

    void setWallpaper(bool enable);
    void setFontSmoothing(bool enable);
    void setEffects(bool enable);

private:
    void disableWallpaper();
    void disableFontSmoothing();
    void disableEffects();

    void revertAll();
    void applyNewSettings();

    bool wallpaper_ = false;
    bool font_smoothing_ = false;
    bool effects_ = false;

    DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

} // namespace desktop

#endif // DESKTOP__WIN__DESKTOP_ENVIRONMENT_H
