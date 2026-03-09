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

#include "base/desktop/desktop_environment_win.h"

#include "base/logging.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"

#include <qt_windows.h>
#include <WtsApi32.h>

namespace base {

namespace {

class ScopedUserImpersonator
{
public:
    ScopedUserImpersonator()
    {
        DWORD session_id = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
        {
            PLOG(ERROR) << "ProcessIdToSessionId failed";
            return;
        }

        ScopedHandle user_token;
        if (!WTSQueryUserToken(session_id, user_token.recieve()))
        {
            PLOG(ERROR) << "WTSQueryUserToken failed";
            return;
        }

        // The process of the desktop session is running with "SYSTEM" account.
        // We need the current real user, not "SYSTEM".
        if (!user_token.isValid())
        {
            LOG(INFO) << "No active user";
            return;
        }

        if (!impersonator_.loggedOnUser(user_token))
        {
            LOG(ERROR) << "loggedOnUser failed";
            return;
        }
    }

private:
    ScopedImpersonator impersonator_;
    Q_DISABLE_COPY_MOVE(ScopedUserImpersonator)
};

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentWin::DesktopEnvironmentWin(QObject* parent)
    : DesktopEnvironment(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentWin::~DesktopEnvironmentWin()
{
    LOG(INFO) << "Dtor";
    revertAll();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableWallpaper()
{
    LOG(INFO) << "Disable desktop wallpaper";
    ScopedUserImpersonator impersonator;

    wchar_t new_path[] = L"";
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, new_path, SPIF_SENDCHANGE);
    wallpaper_changed_ = true;
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableFontSmoothing()
{
    LOG(INFO) << "Disable font smoothing";
    ScopedUserImpersonator impersonator;

    SystemParametersInfoW(SPI_SETFONTSMOOTHING, FALSE, nullptr, SPIF_SENDCHANGE);
    font_smoothing_changed_ = true;
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableEffects()
{
    LOG(INFO) << "Disable desktop effects";
    ScopedUserImpersonator impersonator;

    BOOL drop_shadow = TRUE;
    if (SystemParametersInfoW(SPI_GETDROPSHADOW, 0, &drop_shadow, 0) && drop_shadow)
    {
        SystemParametersInfoW(SPI_SETDROPSHADOW, 0, FALSE, SPIF_SENDCHANGE);
        drop_shadow_changed_ = true;
    }

    ANIMATIONINFO animation;
    animation.cbSize = sizeof(animation);
    if (SystemParametersInfoW(SPI_GETANIMATION, sizeof(animation), &animation, 0) && animation.iMinAnimate)
    {
        animation.iMinAnimate = FALSE;
        SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE);
        animation_changed_ = true;
    }

    BOOL drag_full_windows = TRUE;
    if (SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS, 0, &drag_full_windows, 0) && drag_full_windows)
    {
        SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, FALSE, nullptr, SPIF_SENDCHANGE);
        drag_full_windows_ = true;
    }

    BOOL ui_effects = TRUE;
    if (SystemParametersInfoW(SPI_GETUIEFFECTS, 0, &ui_effects, 0) && ui_effects)
    {
        SystemParametersInfoW(SPI_SETUIEFFECTS, 0, FALSE, SPIF_SENDCHANGE);
        ui_effects_changed_ = true;
    }

    BOOL client_area_animation = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_area_animation, 0) && client_area_animation)
    {
        SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, FALSE, SPIF_SENDCHANGE);
        client_area_animation_changed_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::revertAll()
{
    LOG(INFO) << "Reverting desktop environment changes";
    ScopedUserImpersonator impersonator;

    if (wallpaper_changed_)
    {
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, SETWALLPAPER_DEFAULT, SPIF_SENDCHANGE);
        wallpaper_changed_ = false;
    }

    if (font_smoothing_changed_)
    {
        SystemParametersInfoW(SPI_SETFONTSMOOTHING, TRUE, nullptr, SPIF_SENDCHANGE);
        font_smoothing_changed_ = false;
    }

    if (drag_full_windows_)
    {
        SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, TRUE, 0, SPIF_SENDCHANGE);
        drag_full_windows_ = false;
    }

    if (ui_effects_changed_)
    {
        SystemParametersInfoW(SPI_SETUIEFFECTS, 0, (PVOID)TRUE, SPIF_SENDCHANGE);
        ui_effects_changed_ = false;
    }

    if (client_area_animation_changed_)
    {
        SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, (PVOID)TRUE, SPIF_SENDCHANGE);
        client_area_animation_changed_ = false;
    }

    if (drop_shadow_changed_)
    {
        SystemParametersInfoW(SPI_SETDROPSHADOW, 0, (PVOID)TRUE, SPIF_SENDCHANGE);
        drop_shadow_changed_ = false;
    }

    if (animation_changed_)
    {
        ANIMATIONINFO animation;
        animation.cbSize = sizeof(animation);
        animation.iMinAnimate = TRUE;

        SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE);
        animation_changed_ = false;
    }
}

} // namespace base
