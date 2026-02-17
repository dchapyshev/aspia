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

#ifndef HOST_SYSTEM_SETTINGS_H
#define HOST_SYSTEM_SETTINGS_H

#include <QSettings>

namespace base {
class UserList;
} // namespace peer

namespace host {

class SystemSettings
{
    Q_GADGET

public:
    SystemSettings();
    ~SystemSettings();

    static bool createPasswordHash(const QString& password, QByteArray* hash, QByteArray* salt);
    static bool isValidPassword(const QString& password);

    QString filePath() const;
    bool isWritable() const;
    void sync();

    quint16 tcpPort() const;
    void setTcpPort(quint16 port);

    bool isRouterEnabled() const;
    void setRouterEnabled(bool enable);

    QString routerAddress() const;
    void setRouterAddress(const QString& address);

    quint16 routerPort() const;
    void setRouterPort(quint16 port);

    QByteArray routerPublicKey() const;
    void setRouterPublicKey(const QByteArray& key);

    std::unique_ptr<base::UserList> userList() const;
    void setUserList(const base::UserList& user_list);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

    quint32 preferredVideoCapturer() const;
    void setPreferredVideoCapturer(quint32 type);

    bool passwordProtection() const;
    void setPasswordProtection(bool enable);

    QByteArray passwordHash() const;
    void setPasswordHash(const QByteArray& hash);

    QByteArray passwordHashSalt() const;
    void setPasswordHashSalt(const QByteArray& salt);

    bool oneTimePassword() const;
    void setOneTimePassword(bool enable);

    std::chrono::milliseconds oneTimePasswordExpire() const;
    void setOneTimePasswordExpire(const std::chrono::milliseconds& interval);

    int oneTimePasswordLength() const;
    void setOneTimePasswordLength(int length);

    quint32 oneTimePasswordCharacters() const;
    void setOneTimePasswordCharacters(quint32 characters);

    bool connectConfirmation() const;
    void setConnectConfirmation(bool enable);

    enum class NoUserAction
    {
        ACCEPT = 0,
        REJECT = 1
    };
    Q_ENUM(NoUserAction)

    NoUserAction noUserAction() const;
    void setNoUserAction(NoUserAction action);

    std::chrono::milliseconds autoConfirmationInterval() const;
    void setAutoConfirmationInterval(const std::chrono::milliseconds& interval);

    bool isApplicationShutdownDisabled() const;
    void setApplicationShutdownDisabled(bool value);

    bool isAutoUpdateEnabled() const;
    void setAutoUpdateEnabled(bool enable);

    int updateCheckFrequency() const;
    void setUpdateCheckFrequency(int days);

private:
    mutable QSettings settings_;

    Q_DISABLE_COPY(SystemSettings)
};

} // namespace host

#endif // HOST_SYSTEM_SETTINGS_H
