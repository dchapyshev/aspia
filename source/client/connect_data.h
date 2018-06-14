//
// PROJECT:         Aspia
// FILE:            client/connect_data.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CONNECT_DATA_H
#define _ASPIA_CLIENT__CONNECT_DATA_H

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
    void setDesktopConfig(const proto::desktop::Config& config) { desktop_config_ = config; }

private:
    QString computer_name_;
    QString address_;
    int port_ = 0;
    QString user_name_;
    QString password_;

    proto::auth::SessionType session_type_;
    proto::desktop::Config desktop_config_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CONNECT_DATA_H
