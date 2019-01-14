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

#ifndef ASPIA_HOST__HOST_SETTINGS_H
#define ASPIA_HOST__HOST_SETTINGS_H

#include <QSettings>

#include "base/macros_magic.h"

namespace net {
struct SrpUserList;
} // namespace net

namespace aspia {

class HostSettings
{
public:
    HostSettings();
    ~HostSettings();

    QString filePath() const;
    bool isWritable() const;

    QString locale() const;
    void setLocale(const QString& locale);

    uint16_t tcpPort() const;
    void setTcpPort(uint16_t port);

    bool addFirewallRule() const;
    void setAddFirewallRule(bool value);

    net::SrpUserList userList() const;
    void setUserList(const net::SrpUserList& user_list);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

    bool remoteUpdate() const;
    void setRemoteUpdate(bool allow);

private:
    mutable QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(HostSettings);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SETTINGS_H
