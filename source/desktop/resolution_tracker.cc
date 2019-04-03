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

#include "desktop/resolution_tracker.h"

namespace desktop {

bool ResolutionTracker::setResolution(Size size)
{
    if (!initialized_)
    {
        initialized_ = true;
        last_size_ = size;
        return false;
    }

    if (last_size_ == size)
        return false;

    last_size_ = size;
    return true;
}

void ResolutionTracker::reset()
{
    initialized_ = false;
}

} // namespace desktop
