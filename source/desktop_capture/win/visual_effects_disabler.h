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

#ifndef ASPIA_DESKTOP_CAPTURE__WIN__VISUAL_EFFECTS_DISABLER_H
#define ASPIA_DESKTOP_CAPTURE__WIN__VISUAL_EFFECTS_DISABLER_H

#include <memory>

#include "base/macros_magic.h"

namespace desktop {

struct VisualEffectsState;
struct WallpaperState;

class VisualEffectsDisabler
{
public:
    VisualEffectsDisabler();
    ~VisualEffectsDisabler();

    void disableAll();
    void restoreAll();

    void disableEffects();
    void restoreEffects();

    void disableWallpaper();
    void restoreWallpaper();

    bool isEffectsDisabled() const { return effects_state_ != nullptr; }
    bool isWallpaperDisabled() const { return wallpaper_state_ != nullptr; }

private:
    std::unique_ptr<VisualEffectsState> effects_state_;
    std::unique_ptr<WallpaperState> wallpaper_state_;

    DISALLOW_COPY_AND_ASSIGN(VisualEffectsDisabler);
};

} // namespace desktop

#endif // ASPIA_DESKTOP_CAPTURE__WIN__VISUAL_EFFECTS_DISABLER_H
