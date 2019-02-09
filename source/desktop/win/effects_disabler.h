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

#ifndef DESKTOP__WIN__EFFECTS_DISABLER_H
#define DESKTOP__WIN__EFFECTS_DISABLER_H

#include <memory>

#include "base/macros_magic.h"

namespace desktop {

class EffectsDisabler
{
public:
    EffectsDisabler();
    ~EffectsDisabler();

private:
    enum StateFlags
    {
        STATE_ENABLED = 1,
        STATE_SAVED = 2
    };

    struct State
    {
        uint8_t drag_full_windows = 0;
        uint8_t animation = 0;
        uint8_t menu_animation = 0;
        uint8_t tooltip_animation = 0;
        uint8_t combobox_animation = 0;
        uint8_t selection_fade = 0;
        uint8_t listbox_smooth_scrolling = 0;
        uint8_t ui_effects = 0;
        uint8_t client_area_animation = 0;
        uint8_t gradient_captions = 0;
        uint8_t hot_tracking = 0;
    };

    static std::unique_ptr<State> saveState();
    static void changeState(State* state, bool enable);
    static bool isChangeRequired(uint8_t flags);

    std::unique_ptr<State> state_;

    DISALLOW_COPY_AND_ASSIGN(EffectsDisabler);
};

} // namespace desktop

#endif // DESKTOP__WIN__EFFECTS_DISABLER_H
