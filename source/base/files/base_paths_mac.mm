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

#include "base/files/base_paths.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QString searchPathDirectory(NSSearchPathDirectory directory, NSSearchPathDomainMask domain_mask)
{
    @autoreleasepool
    {
        NSArray<NSString*>* dirs =
            NSSearchPathForDirectoriesInDomains(directory, domain_mask, YES);
        if ([dirs count] < 1)
        {
            LOG(ERROR) << "NSSearchPathForDirectoriesInDomains returned empty array";
            return QString();
        }

        return QString::fromNSString(dirs[0]);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericConfigDir()
{
    return searchPathDirectory(NSApplicationSupportDirectory, NSLocalDomainMask);
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericUserConfigDir()
{
    return searchPathDirectory(NSApplicationSupportDirectory, NSUserDomainMask);
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericDataDir()
{
    return searchPathDirectory(NSApplicationSupportDirectory, NSLocalDomainMask);
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericUserDataDir()
{
    return searchPathDirectory(NSApplicationSupportDirectory, NSUserDomainMask);
}

} // namespace base
