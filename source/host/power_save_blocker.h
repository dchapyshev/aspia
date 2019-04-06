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

#ifndef HOST__POWER_SAVER_BLOCKER_H
#define HOST__POWER_SAVER_BLOCKER_H

#include "base/win/scoped_object.h"

namespace host {

class PowerSaveBlocker
{
public:
    PowerSaveBlocker();
    ~PowerSaveBlocker();

private:
    base::win::ScopedHandle handle_;

    DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

} // namespace host

#endif // HOST__POWER_SAVER_BLOCKER_H
