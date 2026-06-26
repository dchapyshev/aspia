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

#include "base/desktop/desktop_resizer_vt.h"

#include <algorithm>

#include "base/logging.h"
#include "base/desktop/vt_session.h"

namespace {

// Terminal grids (columns x rows) offered to the client as selectable resolutions.
const struct GridSize
{
    int cols;
    int rows;
} kGrids[] =
{
    {  80, 25 }, { 100, 30 }, { 120, 40 }, { 132, 43 }, { 160, 50 }, { 200, 60 }, { 240, 67 }
};

const int kMinCols = 40;
const int kMaxCols = 240;
const int kMinRows = 10;
const int kMaxRows = 75;
const int kDefaultCols = 80;
const int kDefaultRows = 25;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopResizerVt::DesktopResizerVt(VtSession* session, const QSize& cell_size)
    : session_(session),
      cell_(cell_size)
{
    if (cell_.width() <= 0)
        cell_.setWidth(8);
    if (cell_.height() <= 0)
        cell_.setHeight(16);
}

//--------------------------------------------------------------------------------------------------
QList<QSize> DesktopResizerVt::supportedResolutions(ScreenId /* screen_id */)
{
    QList<QSize> resolutions;
    for (const GridSize& grid : kGrids)
        resolutions.append(QSize(grid.cols * cell_.width(), grid.rows * cell_.height()));
    return resolutions;
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerVt::setResolution(ScreenId /* screen_id */, const QSize& resolution)
{
    if (!session_ || resolution.width() <= 0 || resolution.height() <= 0)
        return false;

    const int cols = std::clamp(resolution.width() / cell_.width(), kMinCols, kMaxCols);
    const int rows = std::clamp(resolution.height() / cell_.height(), kMinRows, kMaxRows);

    return session_->resize(rows, cols);
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerVt::restoreResolution(ScreenId /* screen_id */)
{
    if (session_)
        session_->resize(kDefaultRows, kDefaultCols);
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerVt::restoreResulution()
{
    restoreResolution(0);
}
