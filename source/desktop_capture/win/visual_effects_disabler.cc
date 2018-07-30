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

#include "desktop_capture/win/visual_effects_disabler.h"

#include <qt_windows.h>

#include "base/errno_logging.h"

namespace aspia {

struct VisualEffectsState
{
    uint8_t drag_full_windows = 0;
    uint8_t animation = 0;
    uint8_t menu_animation = 0;
    uint8_t tooltip_animation = 0;
    uint8_t combobox_animation = 0;
    uint8_t selection_fade = 0;
    uint8_t listbox_smooth_scrolling = 0;
    uint8_t ui_effects = 0;
};

struct WallpaperState
{
    wchar_t path[MAX_PATH] = { 0 };
};

namespace {

enum StateFlags
{
    STATE_ENABLED = 1,
    STATE_SAVED   = 2,
    STATE_CHANGED = 4,
    STATE_CHANGE_ALLOWED = (STATE_ENABLED | STATE_SAVED),
};

std::unique_ptr<VisualEffectsState> currentEffectsState()
{
    std::unique_ptr<VisualEffectsState> state = std::make_unique<VisualEffectsState>();

    BOOL drag_full_windows;
    if (SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS, 0, &drag_full_windows, 0))
    {
        state->drag_full_windows |= (drag_full_windows ? STATE_ENABLED : 0);
        state->drag_full_windows |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS) failed");
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
        qWarningErrno("SystemParametersInfoW(SPI_GETANIMATION) failed");
    }

    BOOL menu_animation;
    if (SystemParametersInfoW(SPI_GETMENUANIMATION, 0, &menu_animation, 0))
    {
        state->menu_animation |= (menu_animation ? STATE_ENABLED : 0);
        state->menu_animation |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETMENUANIMATION) failed");
    }

    BOOL tooltip_animation;
    if (SystemParametersInfoW(SPI_GETTOOLTIPANIMATION, 0, &tooltip_animation, 0))
    {
        state->tooltip_animation |= (tooltip_animation ? STATE_ENABLED : 0);
        state->tooltip_animation |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETTOOLTIPANIMATION) failed");
    }

    BOOL combobox_animation;
    if (SystemParametersInfoW(SPI_GETCOMBOBOXANIMATION, 0, &combobox_animation, 0))
    {
        state->combobox_animation |= (combobox_animation ? STATE_ENABLED : 0);
        state->combobox_animation |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETCOMBOBOXANIMATION) failed");
    }

    BOOL selection_fade;
    if (SystemParametersInfoW(SPI_GETSELECTIONFADE, 0, &selection_fade, 0))
    {
        state->selection_fade |= (selection_fade ? STATE_ENABLED : 0);
        state->selection_fade |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETSELECTIONFADE) failed");
    }

    BOOL listbox_smooth_scrolling;
    if (SystemParametersInfoW(SPI_GETLISTBOXSMOOTHSCROLLING, 0, &listbox_smooth_scrolling, 0))
    {
        state->listbox_smooth_scrolling |= (listbox_smooth_scrolling ? STATE_ENABLED : 0);
        state->listbox_smooth_scrolling |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETLISTBOXSMOOTHSCROLLING) failed");
    }

    BOOL ui_effects;
    if (SystemParametersInfoW(SPI_GETUIEFFECTS, 0, &ui_effects, 0))
    {
        state->ui_effects |= (ui_effects ? STATE_ENABLED : 0);
        state->ui_effects |= STATE_SAVED;
    }
    else
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETUIEFFECTS) failed");
    }

    return state;
}

void changeEffectsState(VisualEffectsState* state, BOOL enable)
{
    if (state->drag_full_windows & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, enable, 0, SPIF_SENDCHANGE))
        {
            state->drag_full_windows |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS) failed");
        }
    }

    if (state->animation & STATE_CHANGE_ALLOWED)
    {
        ANIMATIONINFO animation;
        animation.cbSize = sizeof(animation);
        animation.iMinAnimate = enable;

        if (SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE))
        {
            state->animation |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETANIMATION) failed");
        }
    }

    if (state->menu_animation & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETMENUANIMATION,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->menu_animation |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETMENUANIMATION) failed");
        }
    }

    if (state->tooltip_animation & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETTOOLTIPANIMATION,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->tooltip_animation |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETTOOLTIPANIMATION) failed");
        }
    }

    if (state->combobox_animation & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETCOMBOBOXANIMATION,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->combobox_animation |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETCOMBOBOXANIMATION) failed");
        }
    }

    if (state->selection_fade & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETSELECTIONFADE,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->selection_fade |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETSELECTIONFADE) failed");
        }
    }

    if (state->listbox_smooth_scrolling & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->listbox_smooth_scrolling |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING) failed");
        }
    }

    if (state->ui_effects & STATE_CHANGE_ALLOWED)
    {
        if (SystemParametersInfoW(SPI_SETUIEFFECTS,
                                  0,
                                  reinterpret_cast<void*>(enable),
                                  SPIF_SENDCHANGE))
        {
            state->ui_effects |= STATE_CHANGED;
        }
        else
        {
            qWarningErrno("SystemParametersInfoW(SPI_SETUIEFFECTS) failed");
        }
    }
}

} // namespace

VisualEffectsDisabler::VisualEffectsDisabler() = default;
VisualEffectsDisabler::~VisualEffectsDisabler() = default;

void VisualEffectsDisabler::disableAll()
{
    disableEffects();
    disableWallpaper();
}

void VisualEffectsDisabler::restoreAll()
{
    restoreEffects();
    restoreWallpaper();
}

void VisualEffectsDisabler::disableEffects()
{
    effects_state_ = currentEffectsState();
    changeEffectsState(effects_state_.get(), false);
}

void VisualEffectsDisabler::restoreEffects()
{
    changeEffectsState(effects_state_.get(), true);
    effects_state_.reset();
}

void VisualEffectsDisabler::disableWallpaper()
{
    wallpaper_state_ = std::make_unique<WallpaperState>();

    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER,
                               _countof(wallpaper_state_->path),
                               wallpaper_state_->path,
                               0))
    {
        qWarningErrno("SystemParametersInfoW(SPI_GETDESKWALLPAPER) failed");
        wallpaper_state_.reset();
    }
    else
    {
        // If the string is empty, then the desktop wallpaper is not installed.
        if (!wallpaper_state_->path[0])
        {
            wallpaper_state_.reset();
        }
        else
        {
            // We do not check the return value. For SPI_SETDESKWALLPAPER, always returns TRUE.
            SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, L"", SPIF_SENDCHANGE);
        }
    }
}

void VisualEffectsDisabler::restoreWallpaper()
{
    if (!wallpaper_state_)
        return;

    // We do not check the return value. For SPI_SETDESKWALLPAPER, always returns TRUE.
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, wallpaper_state_->path, SPIF_SENDCHANGE);
}

} // namespace aspia
