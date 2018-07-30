//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__HOST_SETTINGS_H_
#define ASPIA_HOST__HOST_SETTINGS_H_

#include <QSettings>

#include "base/macros_magic.h"
#include "host/user.h"

namespace aspia {

class HostSettings
{
public:
    HostSettings();
    ~HostSettings() = default;

    bool isWritable() const;

    static QString defaultLocale();
    QString locale() const;
    void setLocale(const QString& locale);

    int tcpPort() const;
    bool setTcpPort(int port);

    QList<User> userList() const;
    bool setUserList(const QList<User>& user_list);

private:
    mutable QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(HostSettings);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SETTINGS_H_
