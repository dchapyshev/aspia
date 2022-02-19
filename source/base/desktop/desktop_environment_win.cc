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
#include "base/settings/json_settings.h"
#include "base/strings/strcat.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/session_info.h"

#include <Windows.h>
#include <WtsApi32.h>

namespace base {

namespace {

class EnvironmentSettings
{
public:
    EnvironmentSettings()
        : impl_(JsonSettings::Scope::SYSTEM, "aspia", "host_environment")
    {
        // Nothing
    }

    ~EnvironmentSettings() = default;

    std::wstring wallpaper() const
    {
        std::string username = userName();
        if (username.empty())
        {
            LOG(LS_INFO) << "No logged in user";
            return std::wstring();
        }

        std::wstring ret = impl_.get<std::wstring>(strCat({ username, "/wallpaper" }));
        LOG(LS_INFO) << "Received wallpaper path: '" << ret << "' (username: '" << username << "')";
        return ret;
    }

    void setWallpaper(const std::wstring& path)
    {
        std::string username = userName();
        if (username.empty())
        {
            LOG(LS_INFO) << "No logged in user";
            return;
        }

        LOG(LS_INFO) << "Store wallpaper path: '" << path << "' (username: '" << username << "')";
        impl_.set(strCat({ username, "/wallpaper" }), path);
    }

private:
    static std::string userName()
    {
        DWORD session_id = 0;

        if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
        {
            PLOG(LS_WARNING) << "ProcessIdToSessionId failed";
            return std::string();
        }

        win::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_WARNING) << "Unable to get session info";
            return std::string();
        }

        return session_info.userName();
    }

    JsonSettings impl_;
    DISALLOW_COPY_AND_ASSIGN(EnvironmentSettings);
};

} // namespace

DesktopEnvironmentWin::DesktopEnvironmentWin()
{
    LOG(LS_INFO) << "Ctor";
}

DesktopEnvironmentWin::~DesktopEnvironmentWin()
{
    LOG(LS_INFO) << "Dtor";

    revertAll();

    EnvironmentSettings settings;
    settings.setWallpaper(L"");
}

// static
void DesktopEnvironmentWin::prepareEnvironment()
{
    EnvironmentSettings settings;
    std::wstring path = settings.wallpaper();
    if (path.empty())
    {
        LOG(LS_INFO) << "Empty wallpaper path";
        return;
    }

    if (!SystemParametersInfoW(
        SPI_SETDESKWALLPAPER, 0, path.data(), SPIF_SENDCHANGE | SPIF_UPDATEINIFILE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }
    else
    {
        LOG(LS_INFO) << "Wallpaper restored: " << path;
    }
}

void DesktopEnvironmentWin::disableWallpaper()
{
    LOG(LS_INFO) << "Disable desktop wallpaper";

    wchar_t old_path[MAX_PATH] = { 0 };
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, old_path, 0))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    wchar_t new_path[] = L"";
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, new_path, SPIF_SENDCHANGE))
    {
        PLOG(LS_WARNING) << "SystemParametersInfoW failed";
    }

    if (old_path[0] != 0)
    {
        EnvironmentSettings settings;
        settings.setWallpaper(old_path);
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

    win::ScopedHandle user_token;
    if (!WTSQueryUserToken(WTSGetActiveConsoleSessionId(), user_token.recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
        return;
    }

    {
        // The process of the desktop session is running with "SYSTEM" account.
        // We need the current real user, not "SYSTEM".
        win::ScopedImpersonator impersonator;
        if (!impersonator.loggedOnUser(user_token))
        {
            LOG(LS_WARNING) << "loggedOnUser failed";
            return;
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
}

} // namespace base
