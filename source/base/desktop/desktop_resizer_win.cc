//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/desktop/win/screen_capture_utils.h"

#include <qt_windows.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool isModeValid(const DEVMODEW& mode)
{
    const DWORD kRequiredFields =
        DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
        DM_DISPLAYFREQUENCY | DM_DISPLAYORIENTATION;

    return (mode.dmFields & kRequiredFields) == kRequiredFields;
}

//--------------------------------------------------------------------------------------------------
Size resolutionFromMode(const DEVMODEW& mode)
{
    DCHECK(isModeValid(mode));
    return Size(static_cast<qint32>(mode.dmPelsWidth), static_cast<qint32>(mode.dmPelsHeight));
}

} // namespace

//--------------------------------------------------------------------------------------------------
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

    QList<Size> supportedResolutions() const;
    bool setResolution(const Size& resolution);
    void restoreResolution();

private:
    bool modeForResolution(const Size& resolution, DEVMODEW* mode);
    void updateBestModeForResolution(const DEVMODEW& current_mode, const DEVMODEW& candidate_mode);

    const ScreenId screen_id_;
    QString name_;
    Size current_resolution_;
    QMap<Size, DEVMODEW> best_mode_;
};

//--------------------------------------------------------------------------------------------------
DesktopResizerWin::Screen::Screen(ScreenId screen_id)
    : screen_id_(screen_id)
{
    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(nullptr, static_cast<DWORD>(screen_id), &device, 0))
    {
        PLOG(LS_ERROR) << "EnumDisplayDevicesW failed";
        return;
    }

    DEVMODEW current_mode;
    current_mode.dmSize = sizeof(current_mode);
    current_mode.dmDriverExtra = 0;

    if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &current_mode, 0))
    {
        PLOG(LS_ERROR) << "EnumDisplaySettingsExW failed";
        return;
    }

    name_ = QString::fromWCharArray(device.DeviceName);
    current_resolution_.set(static_cast<qint32>(current_mode.dmPelsWidth),
                            static_cast<qint32>(current_mode.dmPelsHeight));

    for (DWORD i = 0; ; ++i)
    {
        DEVMODEW candidate_mode;
        memset(&candidate_mode, 0, sizeof(candidate_mode));
        candidate_mode.dmSize = sizeof(candidate_mode);

        if (!EnumDisplaySettingsExW(device.DeviceName, i, &candidate_mode, 0))
            break;

        updateBestModeForResolution(current_mode, candidate_mode);
    }
}

//--------------------------------------------------------------------------------------------------
QList<Size> DesktopResizerWin::Screen::supportedResolutions() const
{
    QList<Size> result;

    for (auto it = best_mode_.begin(), it_end = best_mode_.end(); it != it_end; ++it)
        result.push_back(it.key());

    return result;
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerWin::Screen::setResolution(const Size& resolution)
{
    if (current_resolution_ == resolution)
    {
        LOG(LS_INFO) << "Screen #" << screen_id_ << " already has specified resolution ("
                     << resolution << ")";
        return false;
    }

    DEVMODEW mode;
    if (!modeForResolution(resolution, &mode))
    {
        LOG(LS_ERROR) << "Specified mode " << resolution << " for screen "
                      << screen_id_ << " not found";
        return false;
    }

    LONG result = ChangeDisplaySettingsExW(qUtf16Printable(name_), &mode, nullptr, 0, nullptr);
    if (result != DISP_CHANGE_SUCCESSFUL)
    {
        LOG(LS_ERROR) << "ChangeDisplaySettingsW failed: " << result;
        return false;
    }

    current_resolution_ = resolution;
    return true;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerWin::Screen::restoreResolution()
{
    // Restore the display mode based on the registry configuration.
    LONG result = ChangeDisplaySettingsExW(qUtf16Printable(name_), nullptr, nullptr, 0, nullptr);
    if (result != DISP_CHANGE_SUCCESSFUL)
    {
        LOG(LS_ERROR) << "ChangeDisplaySettingsW failed: " << result;
    }
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerWin::Screen::modeForResolution(const Size& resolution, DEVMODEW* mode)
{
    DCHECK(mode);

    auto result = best_mode_.find(resolution);
    if (result == best_mode_.end())
        return false;

    *mode = result.value();
    return true;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerWin::Screen::updateBestModeForResolution(
    const DEVMODEW& current_mode, const DEVMODEW& candidate_mode)
{
    // Ignore modes missing the fields that we expect.
    if (!isModeValid(candidate_mode))
    {
        LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                     << candidate_mode.dmPelsHeight << ": invalid fields " << Qt::hex
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
    if (!candidate_mode.dmPelsWidth || !candidate_mode.dmPelsHeight)
        return;

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
            //LOG(LS_INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
            //             << candidate_mode.dmPelsHeight
            //             << ": mode matching current frequency already found.";
            return;
        }
    }

    // If we haven't seen this resolution before, or if it's a better match than one we enumerated
    // previously, save it.
    best_mode_[candidate_resolution] = candidate_mode;
}

//--------------------------------------------------------------------------------------------------
DesktopResizerWin::DesktopResizerWin()
{
    ScreenCapturer::ScreenList screen_list;
    if (!ScreenCaptureUtils::screenList(&screen_list))
    {
        LOG(LS_ERROR) << "ScreenCaptureUtils::screenList failed";
        return;
    }

    for (const auto& screen : std::as_const(screen_list.screens))
        screens_.insert(std::make_pair(screen.id, Screen(screen.id)));
}

//--------------------------------------------------------------------------------------------------
DesktopResizerWin::~DesktopResizerWin()
{
    restoreResulution();
}

//--------------------------------------------------------------------------------------------------
QList<Size> DesktopResizerWin::supportedResolutions(ScreenId screen_id)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_ERROR) << "Specified screen not found: " << screen_id;
        return {};
    }

    return screen->second.supportedResolutions();
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerWin::setResolution(ScreenId screen_id, const Size& resolution)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_ERROR) << "Specified screen not found: " << screen_id;
        return false;
    }

    return screen->second.setResolution(resolution);
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerWin::restoreResolution(ScreenId screen_id)
{
    auto screen = screens_.find(screen_id);
    if (screen == screens_.end())
    {
        LOG(LS_ERROR) << "Specified screen not found: " << screen_id;
        return;
    }

    screen->second.restoreResolution();
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerWin::restoreResulution()
{
    for (const auto& screen : screens_)
        restoreResolution(screen.first);
}

} // namespace base
