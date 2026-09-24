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
#include <QObject>
#include <QString>

#include <optional>
#include <utility>

#include "base/sql/sql_database.h"
#include "client/auto_backup.h"
#include "client/config.h"

class Database
{
    Q_GADGET

public:
    // A default-constructed Database is not connected; open() (or instance(), which opens the
    // per-thread connection itself) must be called first.
    Database() = default;
    ~Database() = default;

    static Database& instance();
    static QString filePath();

    // Opens (creating when absent) the database at |file_path| and ensures the schema. instance()
    // opens the per-thread connection at filePath(); this entry exists so the tests can run every
    // path against an isolated temporary database.
    bool open(const QString& file_path);

    bool isValid() const;

    enum class ReadResult { OK, INCOMPLETE, FAILED };
    Q_ENUM(ReadResult)

    enum class FindResult { FOUND, NOT_FOUND, UNREADABLE, FAILED };
    Q_ENUM(FindResult)

    // Local Hosts.
    ReadResult localHostList(qint64 group_id, QList<LocalHostConfig>* hosts) const;
    ReadResult allLocalHosts(QList<LocalHostConfig>* hosts) const;
    bool addLocalHost(LocalHostConfig& host);
    bool modifyLocalHost(LocalHostConfig& host);
    bool moveLocalHost(qint64 entry_id, qint64 new_group_id);
    bool removeLocalHost(qint64 entry_id);
    bool setLocalHostConnectTime(qint64 entry_id, qint64 connect_time);
    FindResult findLocalHost(qint64 entry_id, LocalHostConfig* host) const;
    FindResult findLocalHostByGuid(const QString& guid, LocalHostConfig* host) const;
    std::optional<std::pair<QString, SecureString>> localHostCredentials(qint64 entry_id) const;

    // Local Search.
    ReadResult searchLocalHosts(const QString& query, QList<LocalHostConfig>* hosts) const;

    // Local Groups.
    bool localGroupList(qint64 parent_id, QList<LocalGroupConfig>* groups) const;
    bool allLocalGroups(QList<LocalGroupConfig>* groups) const;
    bool addLocalGroup(LocalGroupConfig& group);
    bool modifyLocalGroup(const LocalGroupConfig& group);
    bool moveLocalGroup(qint64 group_id, qint64 new_parent_id);
    bool removeLocalGroup(qint64 group_id);
    FindResult findLocalGroup(qint64 group_id, LocalGroupConfig* group) const;

    // Routers.
    ReadResult routerList(QList<RouterConfig>* routers) const;
    bool addRouter(RouterConfig& router);
    bool modifyRouter(const RouterConfig& router);
    bool removeRouter(qint64 router_id);
    FindResult findRouter(qint64 router_id, RouterConfig* router) const;

    // Router Hosts.
    ReadResult allRouterHosts(QList<RouterHostConfig>* hosts) const;
    bool addRouterHost(const RouterHostConfig& host);
    bool modifyRouterHost(const RouterHostConfig& host);
    bool removeRouterHost(qint64 router_id, HostId host_id);
    FindResult findRouterHost(qint64 router_id, HostId host_id, RouterHostConfig* host) const;
    std::optional<std::pair<QString, SecureString>> routerHostCredentials(
        qint64 router_id, HostId host_id) const;
    bool outdatedRouterHosts(qint64 router_id, QList<HostId>* hosts) const;
    bool updateRouterHostCheckTime(qint64 router_id, HostId host_id);

    // Credentials.
    ReadResult credentialList(QList<CredentialConfig>* credentials) const;
    bool addCredential(CredentialConfig& credential);
    bool modifyCredential(const CredentialConfig& credential);
    bool removeCredential(qint64 credential_id);
    FindResult findCredential(qint64 credential_id, CredentialConfig* credential) const;
    FindResult findCredentialByGuid(const QString& guid, CredentialConfig* credential) const;

    // Puts these records in place of the address book, all of them or none. Everything the book
    // held is deleted first. A record is named by a key of its own instead of an id, negative and
    // unique among the lists, and the records linking to it carry that key. A parent comes before
    // the records naming it, and the credentials before the hosts entered with them.
    bool import(const QList<RouterConfig>& routers,
                const QList<LocalGroupConfig>& local_groups,
                const QList<LocalHostConfig>& local_hosts,
                const QList<RouterHostConfig>& router_hosts,
                const QList<CredentialConfig>& credentials);

    // Settings.
    QString displayName() const;
    bool setDisplayName(const QString& name);

    bool isCheckUpdatesEnabled() const;
    bool setCheckUpdatesEnabled(bool enable);

    QString updateChannel() const;
    bool setUpdateChannel(const QString& channel);

    bool isBackupOnStartupEnabled() const;
    bool setBackupOnStartupEnabled(bool enable);

    QString backupPath() const;
    bool setBackupPath(const QString& path);

    AutoBackup::Retention backupRetention() const;
    bool setBackupRetention(AutoBackup::Retention retention);

    // Master password.
    bool isMasterPasswordSet() const;

    // Atomically rewrites every stored record with fields already re-encrypted under the new key and
    // updates the master password verifier. Either all changes are applied or none of them are, so
    // the address book can never be left with records under two different keys.
    bool reencryptAll(const QList<LocalHostConfig>& local_hosts,
                      const QList<RouterConfig>& routers,
                      const QList<RouterHostConfig>& router_hosts,
                      const QList<CredentialConfig>& credentials,
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
    friend class DatabaseTestPeer;

    static void setFilePathForTesting(const QString& file_path);

    bool openDatabase();
    bool setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version);

    QString readSetting(const QString& name) const;
    bool writeSetting(const QString& name, const QString& value);

    mutable SqlDatabase db_;
};

#endif // CLIENT_DATABASE_H
