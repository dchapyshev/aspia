//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__DESKTOP_CONFIG_TRACKER_H
#define HOST__DESKTOP_CONFIG_TRACKER_H

#include "base/macros_magic.h"
#include "proto/desktop_session.pb.h"

namespace host {

class DesktopConfigTracker
{
public:
    DesktopConfigTracker() = default;

    enum Flags
    {
        VIDEO_CHANGES     = 1,
        CLIPBOARD_CHANGES = 2,
        EFFECTS_CHANGES   = 4,
        INPUT_CHANGES     = 8,
        ALL_CHANGES       = std::numeric_limits<uint32_t>::max()
    };

    uint32_t changeFlags(const proto::desktop::Config& config);

private:
    proto::desktop::Config old_config_;
    bool has_config_ = false;

    DISALLOW_COPY_AND_ASSIGN(DesktopConfigTracker);
};

} // namespace host

#endif // HOST__DESKTOP_CONFIG_TRACKER_H
