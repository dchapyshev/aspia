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

#include "desktop/win/effects_disabler.h"
#include "base/logging.h"

#include <Windows.h>

namespace desktop {

EffectsDisabler::EffectsDisabler()
{
    state_ = saveState();
    changeState(state_.get(), false);
}

EffectsDisabler::~EffectsDisabler()
{
    changeState(state_.get(), true);
    state_.reset();
}

// static
std::unique_ptr<EffectsDisabler::State> EffectsDisabler::saveState()
{
    std::unique_ptr<State> state = std::make_unique<State>();

    BOOL drag_full_windows;
    if (SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS, 0, &drag_full_windows, 0))
    {
        state->drag_full_windows |= (drag_full_windows ? STATE_ENABLED : 0);
        state->drag_full_windows |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS) failed";
    }

    ANIMATIONINFO animation;
    animation.cbSize = sizeof(ANIMATIONINFO);
    if (SystemParametersInfoW(SPI_GETANIMATION, sizeof(animation), &animation, 0))
    {
        state->animation |= (animation.iMinAnimate != 0) ? STATE_ENABLED : 0;
        state->animation |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETANIMATION) failed";
    }

    BOOL menu_animation;
    if (SystemParametersInfoW(SPI_GETMENUANIMATION, 0, &menu_animation, 0))
    {
        state->menu_animation |= (menu_animation ? STATE_ENABLED : 0);
        state->menu_animation |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETMENUANIMATION) failed";
    }

    BOOL tooltip_animation;
    if (SystemParametersInfoW(SPI_GETTOOLTIPANIMATION, 0, &tooltip_animation, 0))
    {
        state->tooltip_animation |= (tooltip_animation ? STATE_ENABLED : 0);
        state->tooltip_animation |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETTOOLTIPANIMATION) failed";
    }

    BOOL combobox_animation;
    if (SystemParametersInfoW(SPI_GETCOMBOBOXANIMATION, 0, &combobox_animation, 0))
    {
        state->combobox_animation |= (combobox_animation ? STATE_ENABLED : 0);
        state->combobox_animation |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETCOMBOBOXANIMATION) failed";
    }

    BOOL selection_fade;
    if (SystemParametersInfoW(SPI_GETSELECTIONFADE, 0, &selection_fade, 0))
    {
        state->selection_fade |= (selection_fade ? STATE_ENABLED : 0);
        state->selection_fade |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETSELECTIONFADE) failed";
    }

    BOOL listbox_smooth_scrolling;
    if (SystemParametersInfoW(SPI_GETLISTBOXSMOOTHSCROLLING, 0, &listbox_smooth_scrolling, 0))
    {
        state->listbox_smooth_scrolling |= (listbox_smooth_scrolling ? STATE_ENABLED : 0);
        state->listbox_smooth_scrolling |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETLISTBOXSMOOTHSCROLLING) failed";
    }

    BOOL ui_effects;
    if (SystemParametersInfoW(SPI_GETUIEFFECTS, 0, &ui_effects, 0))
    {
        state->ui_effects |= (ui_effects ? STATE_ENABLED : 0);
        state->ui_effects |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETUIEFFECTS) failed";
    }

    BOOL client_area_animation;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_area_animation, 0))
    {
        state->client_area_animation |= (client_area_animation ? STATE_ENABLED : 0);
        state->client_area_animation |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION) failed";
    }

    BOOL gradient_captions;
    if (SystemParametersInfoW(SPI_GETGRADIENTCAPTIONS, 0, &gradient_captions, 0))
    {
        state->gradient_captions |= (gradient_captions ? STATE_ENABLED : 0);
        state->gradient_captions |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETGRADIENTCAPTIONS) failed";
    }

    BOOL hot_tracking;
    if (SystemParametersInfoW(SPI_GETHOTTRACKING, 0, &hot_tracking, 0))
    {
        state->hot_tracking |= (hot_tracking ? STATE_ENABLED : 0);
        state->hot_tracking |= STATE_SAVED;
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETHOTTRACKING) failed";
    }

    return state;
}

// static
void EffectsDisabler::changeState(State* state, bool value)
{
    BOOL enable = !!value;

    if (isChangeRequired(state->drag_full_windows))
    {
        if (!SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, enable, 0, SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS) failed";
        }
    }

    if (isChangeRequired(state->animation))
    {
        ANIMATIONINFO animation;
        animation.cbSize = sizeof(animation);
        animation.iMinAnimate = enable;

        if (!SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETANIMATION) failed";
        }
    }

    if (isChangeRequired(state->menu_animation))
    {
        if (!SystemParametersInfoW(SPI_SETMENUANIMATION,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETMENUANIMATION) failed";
        }
    }

    if (isChangeRequired(state->tooltip_animation))
    {
        if (!SystemParametersInfoW(SPI_SETTOOLTIPANIMATION,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETTOOLTIPANIMATION) failed";
        }
    }

    if (isChangeRequired(state->combobox_animation))
    {
        if (!SystemParametersInfoW(SPI_SETCOMBOBOXANIMATION,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETCOMBOBOXANIMATION) failed";
        }
    }

    if (isChangeRequired(state->selection_fade))
    {
        if (!SystemParametersInfoW(SPI_SETSELECTIONFADE,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETSELECTIONFADE) failed";
        }
    }

    if (isChangeRequired(state->listbox_smooth_scrolling))
    {
        if (!SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING) failed";
        }
    }

    if (isChangeRequired(state->ui_effects))
    {
        if (!SystemParametersInfoW(SPI_SETUIEFFECTS,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETUIEFFECTS) failed";
        }
    }

    if (isChangeRequired(state->client_area_animation))
    {
        if (!SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION) failed";
        }
    }

    if (isChangeRequired(state->gradient_captions))
    {
        if (!SystemParametersInfoW(SPI_SETGRADIENTCAPTIONS,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETGRADIENTCAPTIONS) failed";
        }
    }

    if (isChangeRequired(state->hot_tracking))
    {
        if (!SystemParametersInfoW(SPI_SETHOTTRACKING,
                                   0,
                                   reinterpret_cast<void*>(enable),
                                   SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_SETHOTTRACKING) failed";
        }
    }
}

// static
bool EffectsDisabler::isChangeRequired(uint8_t flags)
{
    return (flags & STATE_ENABLED) && (flags & STATE_SAVED);
}

} // namespace desktop
