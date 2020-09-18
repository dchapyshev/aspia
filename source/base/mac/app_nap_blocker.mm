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

#include "base/mac/app_nap_blocker.h"

#include <Cocoa/Cocoa.h>

namespace base {

AppNapBlocker::AppNapBlocker() = default;

AppNapBlocker::~AppNapBlocker()
{
    unblock();
}

void AppNapBlocker::block()
{
    if (id_)
        return;

    id_ = [[NSProcessInfo processInfo]
        beginActivityWithOptions: NSActivityUserInitiated
        reason: @"Aspia connection"];
    [id_ retain];
}

void AppNapBlocker::unblock()
{
    if (!id_)
        return;

    [[NSProcessInfo processInfo] endActivity:id_];
    [id_ release];
    id_ = nullptr;
}

} // namespace base
