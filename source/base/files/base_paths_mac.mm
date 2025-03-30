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

#include "base/files/base_paths.h"

#include "base/logging.h"
#include "base/mac/nsstring_conversions.h"

#include <Foundation/Foundation.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool searchPathDirectory(NSSearchPathDirectory directory,
                         NSSearchPathDomainMask domain_mask,
                         std::filesystem::path* result)
{
    DCHECK(result);
    NSArray<NSString*>* dirs = NSSearchPathForDirectoriesInDomains(directory, domain_mask, YES);
    if ([dirs count] < 1)
        return false;

    *result = NSStringToUtf8(dirs[0]);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool userDirectory(NSSearchPathDirectory directory, std::filesystem::path* result)
{
    return searchPathDirectory(directory, NSUserDomainMask, result);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::userAppData(std::filesystem::path* result)
{
    return userDirectory(NSDesktopDirectory, result);
}

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::userDesktop(std::filesystem::path* result)
{
    return userDirectory(NSDesktopDirectory, result);
}

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::commonAppData(std::filesystem::path* result)
{
    return userDirectory(NSApplicationSupportDirectory, result);
}

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::commonDesktop(std::filesystem::path* result)
{
    return userDirectory(NSDesktopDirectory, result);
}

} // namespace base
