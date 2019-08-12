//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__SCREEN_CONTROLS_IPC_H
#define HOST__SCREEN_CONTROLS_IPC_H

#include "host/screen_controls.h"

namespace host {

class ScreenControlsIpc : public ScreenControls
{
public:
    ~ScreenControlsIpc() = default;

    // ScreenControls implementation.
    void setScreenResolution(const desktop::Size& resolution) override;
    void setEffectsState(bool enable) override;
    void setWallpaperState(bool enable) override;
};

} // namespace host

#endif // HOST__SCREEN_CONTROLS_IPC_H
