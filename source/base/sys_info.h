//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_SYS_INFO_H
#define BASE_SYS_INFO_H

#include "base/macros_magic.h"

#include <cstdint>
#include <string>

namespace base {

class SysInfo
{
public:
    static std::u16string operatingSystemName();
    static std::u16string operatingSystemVersion();
    static std::u16string operatingSystemArchitecture();
    static std::u16string operatingSystemDir();
    static std::u16string operatingSystemKey();
    static int64_t operatingSystemInstallDate();

    static uint64_t uptime();

    static std::u16string computerName();
    static std::u16string computerDomain();
    static std::u16string computerWorkgroup();

    static std::u16string processorName();
    static std::u16string processorVendor();
    static int processorPackages();
    static int processorCores();
    static int processorThreads();

private:
    DISALLOW_COPY_AND_ASSIGN(SysInfo);
};

} // namespace base

#endif // BASE_SYS_INFO_H
