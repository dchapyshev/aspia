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

#include "base/win/scoped_thread_desktop.h"

#include <utility>

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedThreadDesktop::ScopedThreadDesktop()
    : initial_(Desktop::threadDesktop())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScopedThreadDesktop::~ScopedThreadDesktop()
{
    revert();
}

//--------------------------------------------------------------------------------------------------
bool ScopedThreadDesktop::isSame(const Desktop& desktop) const
{
    if (assigned_.isValid())
        return assigned_.isSame(desktop);

    return initial_.isSame(desktop);
}

//--------------------------------------------------------------------------------------------------
void ScopedThreadDesktop::revert()
{
    if (assigned_.isValid())
    {
        initial_.setThreadDesktop();
        assigned_.close();
    }
}

//--------------------------------------------------------------------------------------------------
bool ScopedThreadDesktop::setThreadDesktop(Desktop&& desktop)
{
    revert();

    if (initial_.isSame(desktop))
        return true;

    if (!desktop.setThreadDesktop())
        return false;

    assigned_ = std::move(desktop);

    return true;
}

} // namespace base
