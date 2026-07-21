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

#ifndef CLIENT_DATABASE_H
#define CLIENT_DATABASE_H

#include <QList>
#include <QString>

#include <optional>

#include "base/sql/sql_database.h"
#include "client/config.h"

class Database
{
public:
    ~Database() = default;

    static Database& instance();
    static QString filePath();
    bool isValid() const;

    // Hosts.
    QList<HostConfig> hostList(qint64 group_id) const;
    QList<HostConfig> allHosts() const;
    bool addHost(HostConfig& host);
    bool modifyHost(HostConfig& host);
    bool removeHost(qint64 entry_id);
    bool setConnectTime(qint64 entry_id, qint64 connect_time);
    std::optional<HostConfig> findHost(qint64 entry_id) const;
    std::optional<HostConfig> findHostByGuid(const QString& guid) const;

    // Search.
    QList<HostConfig> searchHosts(const QString& query) const;

    // Groups.
    QList<GroupConfig> groupList(qint64 parent_id) const;
    QList<GroupConfig> allGroups() const;
    bool addGroup(GroupConfig& group);
    bool modifyGroup(const GroupConfig& group);
    bool moveGroup(qint64 group_id, qint64 new_parent_id);
    bool removeGroup(qint64 group_id);
    std::optional<GroupConfig> findGroup(qint64 group_id) const;

    // Routers.
    QList<RouterConfig> routerList() const;
    bool addRouter(RouterConfig& router);
    bool modifyRouter(const RouterConfig& router);
    bool removeRouter(qint64 router_id);
    std::optional<RouterConfig> findRouter(qint64 router_id) const;

    // Settings.
    QString displayName() const;
    bool setDisplayName(const QString& name);

    bool isCheckUpdatesEnabled() const;
    bool setCheckUpdatesEnabled(bool enable);

    QString updateServer() const;
    bool setUpdateServer(const QString& server);

    // Master password.
    bool isMasterPasswordSet() const;
    bool setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version);

    // Atomically rewrites every stored record with fields already re-encrypted under the new key and
    // updates the master password verifier. Either all changes are applied or none of them are, so
    // the address book can never be left with records under two different keys.
    bool reencryptAll(const QList<HostConfig>& hosts,
                      const QList<GroupConfig>& groups,
                      const QList<RouterConfig>& routers,
                      const QByteArray& salt,
                      const QByteArray& verifier,
                      quint32 version);
    QByteArray masterPasswordSalt() const;
    QByteArray masterPasswordVerifier() const;
    quint32 masterPasswordVersion() const;

    // Biometric unlock.
    bool isBiometricUnlockEnabled() const;
    QByteArray biometricBlob() const;
    bool setBiometricBlob(const QByteArray& blob);
    bool clearBiometricUnlock();

private:
    Database() = default;

    bool openDatabase();

    QString readSetting(const QString& name) const;
    bool writeSetting(const QString& name, const QString& value);

    mutable SqlDatabase db_;
};

#endif // CLIENT_DATABASE_H
