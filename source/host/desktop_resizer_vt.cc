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

#include "host/desktop_resizer_vt.h"

#include <algorithm>

#include "base/logging.h"
#include "base/desktop/vt_monitors.h"
#include "host/screen_capturer_vt.h"
#include "host/system_settings.h"

namespace {

// Standard resolutions offered to the client. With the renderer's fixed cell these map to whole terminal
// grids, so the on-screen size matches the selection exactly.
const QSize kResolutions[] =
{
    QSize(640, 480), QSize(800, 600), QSize(1280, 720), QSize(1280, 800), QSize(1440, 900),
    QSize(1600, 900), QSize(1920, 1080), QSize(1920, 1200), QSize(2560, 1440)
};

const int kMinCols = 40;
const int kMaxCols = 256;
const int kMinRows = 15;
const int kMaxRows = 80;
const int kDefaultCols = 128; // 1280 x 720
const int kDefaultRows = 36;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopResizerVt::DesktopResizerVt(ScreenCapturerVt* capturer)
    : monitors_(capturer ? capturer->monitors().get() : nullptr),
      cell_(capturer ? capturer->cellSize() : QSize())
{
    if (cell_.width() <= 0)
        cell_.setWidth(8);
    if (cell_.height() <= 0)
        cell_.setHeight(16);

    // Restore the resolution last chosen on this host. The client may override it afterwards via its own
    // preferred resolution; that is fine - the saved value is only the starting point.
    const QSize saved_resolution = SystemSettings().vtResolution();
    if (!saved_resolution.isEmpty() && monitors_)
    {
        for (int i = 0; i < monitors_->count(); ++i)
            resizeSession(i, saved_resolution);
    }
}

//--------------------------------------------------------------------------------------------------
QList<QSize> DesktopResizerVt::supportedResolutions(ScreenId /* screen_id */)
{
    QList<QSize> resolutions;
    for (const QSize& res : kResolutions)
    {
        const int cols = std::clamp(res.width() / cell_.width(), kMinCols, kMaxCols);
        const int rows = std::clamp(res.height() / cell_.height(), kMinRows, kMaxRows);
        const QSize snapped(cols * cell_.width(), rows * cell_.height());
        if (!resolutions.contains(snapped))
            resolutions.append(snapped);
    }
    return resolutions;
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerVt::setResolution(ScreenId screen_id, const QSize& resolution)
{
    if (!resizeSession(screen_id, resolution))
        return false;

    // Remember the chosen resolution so the next VT session starts at the same size.
    SystemSettings settings;
    settings.setVtResolution(resolution);
    settings.sync();
    return true;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerVt::restoreResolution(ScreenId screen_id)
{
    VtSession* session = monitors_ ? monitors_->session(static_cast<int>(screen_id)) : nullptr;
    if (session)
        session->resize(kDefaultRows, kDefaultCols);
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerVt::restoreResulution()
{
    if (!monitors_)
        return;

    for (int i = 0; i < monitors_->count(); ++i)
        restoreResolution(i);
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerVt::resizeSession(ScreenId screen_id, const QSize& resolution)
{
    VtSession* session = monitors_ ? monitors_->session(static_cast<int>(screen_id)) : nullptr;
    if (!session || resolution.width() <= 0 || resolution.height() <= 0)
        return false;

    const int cols = std::clamp(resolution.width() / cell_.width(), kMinCols, kMaxCols);
    const int rows = std::clamp(resolution.height() / cell_.height(), kMinRows, kMaxRows);
    return session->resize(rows, cols);
}
