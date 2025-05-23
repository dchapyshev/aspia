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

#ifndef BASE_SYS_INFO_H
#define BASE_SYS_INFO_H

#include <QString>

#include "base/macros_magic.h"

namespace base {

class SysInfo
{
public:
    static QString operatingSystemName();
    static QString operatingSystemVersion();
    static QString operatingSystemArchitecture();
    static QString operatingSystemDir();
    static QString operatingSystemKey();
    static qint64 operatingSystemInstallDate();

    static quint64 uptime();

    static QString computerName();
    static QString computerDomain();
    static QString computerWorkgroup();

    static QString processorName();
    static QString processorVendor();
    static int processorPackages();
    static int processorCores();
    static int processorThreads();

private:
    DISALLOW_COPY_AND_ASSIGN(SysInfo);
};

} // namespace base

#endif // BASE_SYS_INFO_H
