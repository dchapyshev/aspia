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

#ifndef ASPIA_CLIENT__CONNECT_DATA_H_
#define ASPIA_CLIENT__CONNECT_DATA_H_

#include <QString>

#include "protocol/authorization.pb.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class ConnectData
{
public:
    ConnectData() = default;
    ~ConnectData();

    QString computerName() const { return computer_name_; }
    void setComputerName(const QString& computer_name) { computer_name_ = computer_name; }

    QString address() const { return address_; }
    void setAddress(const QString& address) { address_ = address; }

    int port() const { return port_; }
    void setPort(int port) { port_ = port; }

    QString userName() const { return user_name_; }
    void setUserName(const QString& user_name) { user_name_ = user_name; }

    QString password() const { return password_; }
    void setPassword(const QString& password) { password_ = password; }

    proto::auth::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::auth::SessionType session_type) { session_type_ = session_type; }

    proto::desktop::Config desktopConfig() const { return desktop_config_; }
    void setDesktopConfig(const proto::desktop::Config& config);

private:
    QString computer_name_;
    QString address_;
    int port_ = 0;
    QString user_name_;
    QString password_;

    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    proto::desktop::Config desktop_config_;
};

} // namespace aspia

#endif // ASPIA_CLIENT__CONNECT_DATA_H_
