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

//--------------------------------------------------------------------------------------------------
void updatePerUserSystemParameters()
{
    ScopedHandle user_token;

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(ERROR) << "ProcessIdToSessionId failed";
    }
    else
    {
        if (!WTSQueryUserToken(session_id, user_token.recieve()))
        {
            PLOG(ERROR) << "WTSQueryUserToken failed";
        }
    }

    // The process of the desktop session is running with "SYSTEM" account.
    // We need the current real user, not "SYSTEM".
    ScopedImpersonator impersonator;
    if (user_token.isValid())
    {
        if (!impersonator.loggedOnUser(user_token))
        {
            LOG(ERROR) << "loggedOnUser failed";
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
            static const DWORD kUserLoggedOn = 1;
            static const DWORD kPolicyChange = 2;
            static const DWORD kRemoteSettings = 4;

            DWORD flags = kPolicyChange | kRemoteSettings;
            if (user_token.isValid())
                flags |= kUserLoggedOn;

            // WARNING! Undocumented function!
            // Any ideas how to update user settings without using it?
            if (!update_per_user_system_parameters(flags))
            {
                PLOG(ERROR) << "UpdatePerUserSystemParameters failed";
            }
        }
        else
        {
            PLOG(ERROR) << "GetProcAddress failed";
        }
    }
    else
    {
        PLOG(ERROR) << "GetModuleHandleW failed";
    }
}

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
// static
void DesktopEnvironmentWin::updateEnvironment()
{
    LOG(INFO) << "Updating environment";
    updatePerUserSystemParameters();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableWallpaper()
{
    LOG(INFO) << "Disable desktop wallpaper";

    wchar_t new_path[] = L"";
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, new_path, SPIF_SENDCHANGE))
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableFontSmoothing()
{
    LOG(INFO) << "Disable font smoothing";
    if (!SystemParametersInfoW(SPI_SETFONTSMOOTHING, FALSE, nullptr, SPIF_SENDCHANGE))
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::disableEffects()
{
    LOG(INFO) << "Disable desktop effects";

    BOOL drop_shadow = TRUE;
    if (SystemParametersInfoW(SPI_GETDROPSHADOW, 0, &drop_shadow, 0))
    {
        if (drop_shadow)
        {
            if (!SystemParametersInfoW(SPI_SETDROPSHADOW, 0, FALSE, SPIF_SENDCHANGE))
            {
                PLOG(ERROR) << "SystemParametersInfoW failed";
            }
            drop_shadow_changed_ = true;
        }
    }
    else
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
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
                PLOG(ERROR) << "SystemParametersInfoW failed";
            }
            animation_changed_ = true;
        }
    }
    else
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, FALSE, nullptr, SPIF_SENDCHANGE))
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETUIEFFECTS, 0, FALSE, SPIF_SENDCHANGE))
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }

    if (!SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, FALSE, SPIF_SENDCHANGE))
    {
        PLOG(ERROR) << "SystemParametersInfoW failed";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentWin::revertAll()
{
    LOG(INFO) << "Reverting desktop environment changes";

    if (drop_shadow_changed_)
    {
        if (!SystemParametersInfoW(SPI_SETDROPSHADOW, 0, reinterpret_cast<PVOID>(TRUE), SPIF_SENDCHANGE))
        {
            PLOG(ERROR) << "SystemParametersInfoW failed";
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
            PLOG(ERROR) << "SystemParametersInfoW failed";
        }
        animation_changed_ = false;
    }

    updatePerUserSystemParameters();
}

} // namespace base
