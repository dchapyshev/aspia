//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/desktop_resizer_win.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/desktop/win/screen_capture_utils.h"

#include <Windows.h>

namespace base {

namespace {

bool isModeValid(const DEVMODEW& mode)
{
    const DWORD kRequiredFields =
        DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
        DM_DISPLAYFREQUENCY | DM_DISPLAYORIENTATION;

    return (mode.dmFields & kRequiredFields) == kRequiredFields;
}

Size resolutionFromMode(const DEVMODEW& mode)
{
    DCHECK(isModeValid(mode));
    return Size(static_cast<int32_t>(mode.dmPelsWidth), static_cast<int32_t>(mode.dmPelsHeight));
}

} // namespace

// Provide comparison operation for base::Size so we can use it in std::map.
static inline bool operator<(const Size& a, const Size& b)
{
    if (a.width() != b.width())
        return a.width() < b.width();

    return a.height() < b.height();
}

class DesktopResizerWin::Screen
{
public:
    explicit Screen(ScreenId screen_id);
    ~Screen() = default;

    const std::wstring& deviceName() const;
    bool modeForResolution(const Size& resolution, DEVMODEW* mode);
    std::vector<Size> supportedResolutions() const;

private:
    void updateBestModeForResolution(const DEVMODEW& current_mode, const DEVMODEW& candidate_mode);

    std::wstring name_;
    std::map<Size, DEVMODEW> best_mode_;
};

DesktopResizerWin::Screen::Screen(ScreenId screen_id)
{
    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(nullptr, static_cast<DWORD>(screen_id), &device, 0))
    {
        PLOG(LS_WARNING) << "EnumDisplayDevicesW failed";
        return;
    }

    DEVMODEW current_mode;
    current_mode.dmSize = sizeof(current_mode);
    current_mode.dmDriverExtra = 0;

    if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &current_mode, 0))
    {
        PLOG(LS_WARNING) << "EnumDisplaySettingsExW failed";
        return;
    }

    name_ = device.DeviceName;

    for (DWORD i = 0; ; ++i)
    {
        DEVMODEW candidate_mode;
        memset(&candidate_mode, 0, sizeof(candidate_mode));
        candidate_mode.dmSize = sizeof(candidate_mode);

        if (!EnumDisplaySettingsExW(device.DeviceName, i, &candidate_mode, EDS_ROTATEDMODE))
            break;

        updateBestModeForResolution(current_mode, candidate_mode);
    }
}

const std::wstring& DesktopResizerWin::Screen::deviceName() const
{
    return name_;
}

bool DesktopResizerWin::Screen::modeForResolution(const Size& resolution, DEVMODEW* mode)
{
    DCHECK(mode);

    auto result = best_mode_.find(resolution);
    if (result == best_mode_.end())
        return false;

    *mode = result->second;
    return true;
}

std::vector<Size> DesktopResizerWin::Screen::supportedResolutions() const
{
    std::vector<Size> result;
    result.resize(best_mode_.size());

    for (auto it = best_mode_.begin(); it != best_mode_.end(); ++it)
        result.push_back(it->first);

    return result;
}

void DesktopResizerWin::Screen::updateBestModeForResolution(
    const DEVMODEW& current_mode, const DEVMODEW& candidate_mode)
{
    // Ignore modes missing the fields that we expect.
    if (!isModeValid(candidate_mode))
    {
        LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                     << candidate_mode.dmPelsHeight << ": invalid fields " << std::hex
                     << candidate_mode.dmFields;
        return;
    }

    // Ignore modes with differing bits-per-pixel.
    if (candidate_mode.dmBitsPerPel != current_mode.dmBitsPerPel)
    {
        LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                     << candidate_mode.dmPelsHeight << ": mismatched BPP: expected "
                     << current_mode.dmFields << " but got " << current_mode.dmFields;
        return;
    }

    Size candidate_resolution = resolutionFromMode(candidate_mode);

    if (best_mode_.count(candidate_resolution) != 0)
    {
        DEVMODEW best_mode = best_mode_[candidate_resolution];

        bool best_mode_matches_current_orientation =
            best_mode.dmDisplayOrientation == current_mode.dmDisplayOrientation;
        bool candidate_mode_matches_current_orientation =
            candidate_mode.dmDisplayOrientation == current_mode.dmDisplayOrientation;
        if (best_mode_matches_current_orientation && !candidate_mode_matches_current_orientation)
        {
            LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                         << candidate_mode.dmPelsHeight
                         << ": mode matching current orientation already found.";
            return;
        }

        bool best_mode_matches_current_frequency =
            best_mode.dmDisplayFrequency == current_mode.dmDisplayFrequency;
        bool candidate_mode_matches_current_frequency =
            candidate_mode.dmDisplayFrequency == current_mode.dmDisplayFrequency;
        if (best_mode_matches_current_frequency && !candidate_mode_matches_current_frequency)
        {
            LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                         << candidate_mode.dmPelsHeight
                         << ": mode matching current frequency already found.";
            return;
        }
    }

    // If we haven't seen this resolution before, or if it's a better match than one we enumerated
    // previously, save it.
    best_mode_[candidate_resolution] = candidate_mode;
}

DesktopResizerWin::DesktopResizerWin()
{
    ScreenCapturer::ScreenList screens;
    if (!ScreenCaptureUtils::screenList(&screens))
    {
        LOG(LS_WARNING) << "ScreenCaptureUtils::screenList failed";
        return;
    }

    for (const auto& screen : screens)
        screens_.emplace(screen.id, std::make_unique<Screen>(screen.id));
}

DesktopResizerWin::~DesktopResizerWin()
{
    restureResulution();
}

std::vector<Size> DesktopResizerWin::supportedResolutions(ScreenId screen_id)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_WARNING) << "Specified screen not found: " << screen_id;
        return {};
    }

    return screen->second->supportedResolutions();
}

void DesktopResizerWin::setResolution(ScreenId screen_id, const Size& resolution)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_WARNING) << "Specified screen not found: " << screen_id;
        return;
    }

    Screen* selected_screen = screen->second.get();

    DEVMODEW mode;
    if (!selected_screen->modeForResolution(resolution, &mode))
    {
        LOG(LS_WARNING) << "Specified mode "<< resolution << " for screen "
                        << screen_id << " not found";
        return;
    }

    LONG result = ChangeDisplaySettingsExW(selected_screen->deviceName().c_str(),
                                           &mode, nullptr, 0, nullptr);
    if (result != DISP_CHANGE_SUCCESSFUL)
    {
        LOG(LS_WARNING) << "ChangeDisplaySettingsW failed: " << result;
        return;
    }
}

void DesktopResizerWin::restoreResolution(ScreenId screen_id)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_WARNING) << "Specified screen not found: " << screen_id;
        return;
    }

    Screen* selected_screen = screen->second.get();

    // Restore the display mode based on the registry configuration.
    LONG result = ChangeDisplaySettingsExW(selected_screen->deviceName().c_str(), nullptr, nullptr,
                                           0, nullptr);
    if (result != DISP_CHANGE_SUCCESSFUL)
    {
        LOG(LS_WARNING) << "ChangeDisplaySettingsW failed: " << result;
    }
}

void DesktopResizerWin::restureResulution()
{
    for (const auto& screen : screens_)
        restoreResolution(screen.first);
}

} // namespace base
