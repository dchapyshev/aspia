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

#ifndef BASE_DESKTOP_POWER_SAVE_BLOCKER_H
#define BASE_DESKTOP_POWER_SAVE_BLOCKER_H

#include "base/macros_magic.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#endif // defined(OS_WIN)

namespace base {

class PowerSaveBlocker
{
public:
    PowerSaveBlocker();
    ~PowerSaveBlocker();

private:
#if defined(OS_WIN)
    ScopedHandle handle_;
#endif // defined(OS_WIN)

    DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

} // namespace base

#endif // BASE_DESKTOP_WIN_POWER_SAVE_BLOCKER_H
