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

#include "desktop/win/wallpaper_disabler.h"
#include "base/win/process_util.h"
#include "base/win/scoped_impersonator.h"
#include "base/logging.h"

#include <Windows.h>
#include <wtsapi32.h>

namespace desktop {

namespace {

bool createLoggedOnUserToken(base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    base::win::ScopedImpersonator impersonator;

    if (!impersonator.loggedOnUser(privileged_token))
        return false;

    if (!WTSQueryUserToken(WTSGetActiveConsoleSessionId(), token_out->recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
        return false;
    }

    return true;
}

void updateUserSettings()
{
    base::win::ScopedHandle user_token;

    if (!createLoggedOnUserToken(&user_token))
        return;

    base::win::ScopedImpersonator impersonator;

    // The process of the desktop session is running with "SYSTEM" account.
    // We need the current real user, not "SYSTEM".
    if (!impersonator.loggedOnUser(user_token))
        return;

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
            update_per_user_system_parameters(0x06);
        }
    }
}

} // namespace

WallpaperDisabler::WallpaperDisabler()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, std::size(buffer), buffer, 0))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW(SPI_GETDESKWALLPAPER) failed";
        return;
    }

    // If the string is empty, then the desktop wallpaper is not installed.
    if (!buffer[0])
        return;

    has_wallpaper_ = true;

    // We do not check the return value. For SPI_SETDESKWALLPAPER, always returns TRUE.
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, L"", SPIF_SENDCHANGE);
}

WallpaperDisabler::~WallpaperDisabler()
{
    if (!has_wallpaper_)
        return;

    // We do not check the return value. For SPI_SETDESKWALLPAPER, always returns TRUE.
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, 0, 0);
    updateUserSettings();
}

} // namespace desktop
