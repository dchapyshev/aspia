//
// PROJECT:         Aspia
// FILE:            host/host_settings.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SETTINGS_H
#define _ASPIA_HOST__HOST_SETTINGS_H

#include <QSettings>

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

    Q_DISABLE_COPY(HostSettings)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SETTINGS_H
