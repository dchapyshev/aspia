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

#include "base/desktop/desktop_environment_win.h"

#include "base/logging.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"

#include <Windows.h>
#include <WtsApi32.h>

namespace base {

namespace {

void updatePerUserSystemParameters()
{
    win::ScopedHandle user_token;
    if (!WTSQueryUserToken(WTSGetActiveConsoleSessionId(), user_token.recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
    }

    // The process of the desktop session is running with "SYSTEM" account.
    // We need the current real user, not "SYSTEM".
    win::ScopedImpersonator impersonator;
    if (user_token.isValid())
    {
        if (!impersonator.loggedOnUser(user_token))
        {
            LOG(LS_WARNING) << "loggedOnUser failed";
        }
    }

    HMODULE module = GetModuleHandleW(L"user32.dll");
    if (module)
    {
        // The function prototype is relevant for versions starting from Windows Vista.
        // Older versions have a different prototype.
        typedef BOOL(WINAPI* UpdatePerUserSystemParametersFunc)(DWORD flags);

        UpdatePerUserSystemParametersFunc update_per_user_system_parameters =
            reinterpret_cast<UpdatePerUserSystemParametersFunc>(
                GetProcAddress(module, "UpdatePerUserSystemParameters"));
        if (update_per_user_system_parameters)
        {
            // WARNING! Undocumented function!
            // Any ideas how to update user settings without using it?
            if (!update_per_user_system_parameters(0x06))
            {
                PLOG(LS_WARNING) << "UpdatePerUserSystemParameters failed";
            }
        }
        else
        {
            PLOG(LS_WARNING) << "GetProcAddress failed";
        }
    }
    else
    {
        PLOG(LS_WARNING) << "GetModuleHandleW failed";
    }
}

} // namespace

DesktopEnvironmentWin::DesktopEnvironmentWin()
{
    LOG(LS_INFO) << "Ctor";
}

DesktopEnvironmentWin::~DesktopEnvironmentWin()
{
    LOG(LS_INFO) << "Dtor";

    revertAll();
}

// static
void DesktopEnvironmentWin::updateEnvironment()
{
    updatePerUserSystemParameters();
}

void DesktopEnvironmentWin::disableWallpaper()
{
    LOG(LS_INFO) << "Disable desktop wallpaper";

    wchar_t new_path[] = L"";
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, new_path, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }
}

void DesktopEnvironmentWin::disableFontSmoothing()
{
    LOG(LS_INFO) << "Disable font smoothing";
    if (!SystemParametersInfoW(SPI_SETFONTSMOOTHING, FALSE, nullptr, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }
}

void DesktopEnvironmentWin::disableEffects()
{
    LOG(LS_INFO) << "Disable desktop effects";

    BOOL drop_shadow = TRUE;
    if (SystemParametersInfoW(SPI_GETDROPSHADOW, 0, &drop_shadow, 0))
    {
        if (drop_shadow)
        {
            if (!SystemParametersInfoW(SPI_SETDROPSHADOW, 0, FALSE, SPIF_SENDCHANGE))
            {
                PLOG(LS_WARNING) << "SystemParametersInfoW failed";
            }
            drop_shadow_changed_ = true;
        }
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    ANIMATIONINFO animation;
    animation.cbSize = sizeof(animation);
    if (SystemParametersInfoW(SPI_GETANIMATION, sizeof(animation), &animation, 0))
    {
        if (animation.iMinAnimate)
        {
            animation.iMinAnimate = FALSE;
            if (!SystemParametersInfoW(
                SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE))
            {
                PLOG(LS_WARNING) << "SystemParametersInfoW failed";
            }
            animation_changed_ = true;
        }
    }
    else
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, FALSE, nullptr, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETUIEFFECTS, 0, FALSE, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, FALSE, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }
}

void DesktopEnvironmentWin::revertAll()
{
    LOG(LS_INFO) << "Reverting desktop environment changes";

    if (drop_shadow_changed_)
    {
        if (!SystemParametersInfoW(
            SPI_SETDROPSHADOW, 0, reinterpret_cast<PVOID>(TRUE), SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW failed";
        }
        drop_shadow_changed_ = false;
    }

    if (animation_changed_)
    {
        ANIMATIONINFO animation;
        animation.cbSize = sizeof(animation);
        animation.iMinAnimate = TRUE;

        if (!SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation), &animation, SPIF_SENDCHANGE))
        {
            PLOG(LS_WARNING) << "SystemParametersInfoW failed";
        }
        animation_changed_ = false;
    }

    updatePerUserSystemParameters();
}

} // namespace base
