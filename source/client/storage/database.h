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

#ifndef CLIENT_STORAGE_DATABASE_H
#define CLIENT_STORAGE_DATABASE_H

#include <QList>
#include <QObject>
#include <QString>

#include <optional>
#include <utility>

#include "base/time_types.h"
#include "base/sql/sql_database.h"
#include "client/config.h"
#include "client/storage/auto_backup.h"

class Database
{
    Q_GADGET

public:
    // A default-constructed Database is not connected; open() (or instance(), which opens the
    // per-thread connection itself) must be called first.
    Database() = default;
    ~Database() = default;

    static Database& instance();

    static constexpr char kDirectoryName[] = "client";
    static constexpr char kDefaultFileName[] = "default.db3";

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
    bool setLocalHostOsCredentialId(qint64 entry_id, qint64 credential_id);
    FindResult findLocalHost(qint64 entry_id, LocalHostConfig* host) const;
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

    // Router.
    FindResult routerConfig(RouterConfig* router) const;
    bool setRouterConfig(const RouterConfig& router);

    // Router Hosts.
    ReadResult allRouterHosts(QList<RouterHostConfig>* hosts) const;
    bool addRouterHost(const RouterHostConfig& host);
    bool modifyRouterHost(const RouterHostConfig& host);
    bool removeRouterHost(HostId host_id);
    FindResult findRouterHost(HostId host_id, RouterHostConfig* host) const;
    std::optional<std::pair<QString, SecureString>> routerHostCredentials(HostId host_id) const;
    bool outdatedRouterHosts(QList<HostId>* hosts) const;
    bool updateRouterHostCheckTime(HostId host_id);
    bool setRouterHostOsCredentialId(HostId host_id, qint64 credential_id);
    bool setRouterHostProfileId(HostId host_id, qint64 profile_id);

    // Credentials.
    ReadResult credentialList(QList<CredentialConfig>* credentials,
        std::optional<CredentialConfig::Type> type = std::nullopt) const;
    bool addCredential(CredentialConfig& credential);
    bool modifyCredential(const CredentialConfig& credential);
    bool removeCredential(qint64 credential_id);
    FindResult findCredential(qint64 credential_id, CredentialConfig* credential) const;
    FindResult findCredentialByGuid(const QString& guid, CredentialConfig* credential) const;

    // Connection profiles.
    ReadResult profileList(QList<ProfileConfig>* profiles) const;
    bool addProfile(ProfileConfig& profile);
    bool modifyProfile(const ProfileConfig& profile);
    bool removeProfile(qint64 profile_id);
    FindResult findProfile(qint64 profile_id, ProfileConfig* profile) const;
    ProfileConfig defaultProfile() const;
    ProfileConfig hostProfile(const HostConfig& host) const;

    bool import(const std::optional<RouterConfig>& router,
                const QList<LocalGroupConfig>& local_groups,
                const QList<LocalHostConfig>& local_hosts,
                const QList<RouterHostConfig>& router_hosts,
                const QList<CredentialConfig>& credentials,
                const QList<ProfileConfig>& profiles);

    // On failure the database is left as it was. A file already at |copy_path| is not overwritten.
    bool saveCopyAndClearAll(const QString& copy_path);

    // Settings.
    QString guid() const;

    QString displayName() const;
    bool setDisplayName(const QString& name);

    bool isConfirmCloseEnabled() const;
    bool setConfirmCloseEnabled(bool enable);

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

    Minutes lockTimeout() const;
    bool setLockTimeout(Minutes timeout);

    // Without a saved value the proxy is not used. A value that did not open leaves |proxy| that
    // way and answers INCOMPLETE.
    ReadResult proxyConfig(ProxyConfig* proxy) const;
    bool setProxyConfig(const ProxyConfig& proxy);

    bool isMcpEnabled() const;
    bool setMcpEnabled(bool enable);

    bool isMcpAutoConfirmEnabled() const;
    bool setMcpAutoConfirmEnabled(bool enable);

    bool isMcpSignInEnabled() const;
    bool setMcpSignInEnabled(bool enable);

    quint32 mcpSessionTypes() const;
    bool setMcpSessionTypes(quint32 session_types);

    // Master password.
    bool isMasterPasswordSet() const;

    bool reencryptAll(const QList<LocalHostConfig>& local_hosts,
                      const std::optional<RouterConfig>& router,
                      const QList<RouterHostConfig>& router_hosts,
                      const QList<CredentialConfig>& credentials,
                      const ProxyConfig& proxy,
                      const QByteArray& salt,
                      const QByteArray& verifier,
                      quint32 version);
    QByteArray masterPasswordSalt() const;
    QByteArray masterPasswordVerifier() const;
    quint32 masterPasswordVersion() const;

    // Automatic unlock on startup.
    QByteArray autoUnlockBlob() const;
    bool setAutoUnlockBlob(const QByteArray& blob);

    // Biometric unlock.
    bool isBiometricUnlockEnabled() const;
    QByteArray biometricBlob() const;
    bool setBiometricBlob(const QByteArray& blob);
    bool clearBiometricUnlock();

private:
    friend class DatabaseMigration;
    friend class DatabaseTestPeer;

    static void setFilePathForTesting(const QString& file_path);
    static bool createTables(SqlDatabase& db);

    bool openDatabase();
    bool setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version);

    QString readSetting(const QString& name, bool* found = nullptr) const;
    bool writeSetting(const QString& name, const QString& value);
    bool ensureDefaultProfile();
    bool ensureGuid();
    bool clearRouter();

    mutable SqlDatabase db_;
};

#endif // CLIENT_STORAGE_DATABASE_H
