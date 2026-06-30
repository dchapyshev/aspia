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

#ifndef BASE_SYS_INFO_H
#define BASE_SYS_INFO_H

#include <QList>
#include <QString>

class SysInfo
{
public:
    struct UserGroup
    {
        QString name;
    };

    struct User
    {
        QString name;
        QString full_name;
        QString home_dir;
        QList<UserGroup> groups;
        bool disabled = false;
        bool password_expired = false;
        bool dont_expire_password = false;
        quint64 last_logon_time = 0;
    };

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

    static QByteArray smbiosDump();

    static QList<User> users();
    static QList<UserGroup> userGroups();

private:
    Q_DISABLE_COPY_MOVE(SysInfo)
};

#endif // BASE_SYS_INFO_H
