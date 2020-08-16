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

#include "base/sys_info.h"

#include "base/logging.h"

namespace base {

//static
std::string SysInfo::operatingSystemName()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
std::string SysInfo::operatingSystemVersion()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
std::string SysInfo::operatingSystemArchitecture()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
std::string SysInfo::operatingSystemDir()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
uint64_t SysInfo::uptime()
{
    NOTIMPLEMENTED();
    return 0;
}

// static
std::string SysInfo::computerName()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
std::string SysInfo::computerDomain()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
std::string SysInfo::computerWorkgroup()
{
    NOTIMPLEMENTED();
    return std::string();
}

// static
int SysInfo::processorPackages()
{
    NOTIMPLEMENTED();
    return 0;
}

// static
int SysInfo::processorCores()
{
    NOTIMPLEMENTED();
    return 0;
}

// static
int SysInfo::processorThreads()
{
    NOTIMPLEMENTED();
    return 0;
}

} // namespace base
