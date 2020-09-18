//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__MAC__APP_NAP_BLOCKER_H
#define BASE__MAC__APP_NAP_BLOCKER_H

#include "base/macros_magic.h"

#include <objc/objc.h>

namespace base {

class AppNapBlocker
{
public:
    AppNapBlocker();
    ~AppNapBlocker();

    void block();
    void unblock();

private:
    id id_ = nullptr;
    DISALLOW_COPY_AND_ASSIGN(AppNapBlocker);
};

} // namespace base

#endif // BASE__MAC__APP_NAP_BLOCKER_H
