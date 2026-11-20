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
#include "base/sql/sql_transaction.h"
#include "client/config.h"
#include "client/storage/auto_backup.h"
#include "proto/router_sync.h"

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

    static QString directoryPath();
    static QString filePath();

    struct Info
    {
        QString id;
        QString name;
        bool auto_unlock = false;
        bool has_password = true;
    };

    static QList<Info> list();
    static void select(const QString& id);
    static bool moveToTrash(const QString& id);
    static bool remove(const QString& id);

    // Opens (creating when absent) the database at |file_path| and ensures the schema. instance()
    // opens the per-thread connection at filePath(); this entry exists so the tests can run every
    // path against an isolated temporary database.
    bool open(const QString& file_path);

    bool isValid() const;
    QString guid() const;

    enum class ReadResult { OK, INCOMPLETE, FAILED };
    Q_ENUM(ReadResult)

    enum class FindResult { FOUND, NOT_FOUND, UNREADABLE, FAILED };
    Q_ENUM(FindResult)

    enum class SyncResult { DONE, DETACHED, FAILED };
    Q_ENUM(SyncResult)

    enum class ReencryptResult { DONE, UNREADABLE_RECORD, FAILED };
    Q_ENUM(ReencryptResult)

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
    SyncResult setSyncStorage(const RouterConfig& router, const QByteArray& storage_id,
        const SecureByteArray& key, bool reset);
    bool setSyncStorageResetPending(const RouterConfig& router);

    class SyncTransaction final
    {
    public:
        explicit SyncTransaction(Database& db);

        SyncResult begin(const QByteArray& storage_id);
        bool commit();

        bool storeReceivedCredential(const CredentialConfig& credential, qint64 sync_revision);
        bool setCredentialUnknownData(const QString& guid, const QByteArray& unknown_data);
        bool storeReceivedProfile(const ProfileConfig& profile, qint64 sync_revision);
        bool setProfileUnknownData(const QString& guid, const QByteArray& unknown_data);

        bool eraseRecord(proto::router::SyncDataType type, const QString& guid);
        bool setRecordSyncRevision(proto::router::SyncDataType type, const QString& guid, qint64 sync_revision);
        bool confirmSentRecord(proto::router::SyncDataType type, const QString& guid, qint64 sync_revision,
                               qint64 sync_dirty);
        bool setReceivedRevision(proto::router::SyncDataType type, qint64 revision);

    private:
        bool isActive() const;

        Database& db_;
        std::optional<SqlTransaction> transaction_;

        Q_DISABLE_COPY_MOVE(SyncTransaction)
    };

    qint64 receivedRevision(proto::router::SyncDataType type) const;

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
    struct UnsyncedCredential
    {
        CredentialConfig credential;
        bool deleted = false;
        qint64 sync_revision = 0;
        qint64 sync_dirty = 0;
    };

    ReadResult credentialList(QList<CredentialConfig>* credentials,
        std::optional<CredentialConfig::Type> type = std::nullopt) const;
    bool addCredential(CredentialConfig& credential);
    bool modifyCredential(const CredentialConfig& credential);
    bool removeCredential(qint64 credential_id);
    FindResult findCredential(qint64 credential_id, CredentialConfig* credential) const;
    FindResult findCredentialByGuid(const QString& guid, CredentialConfig* credential) const;
    ReadResult unsyncedCredentials(QList<UnsyncedCredential>* credentials) const;

    // Connection profiles.
    struct UnsyncedProfile
    {
        ProfileConfig profile;
        bool deleted = false;
        bool damaged = false;
        qint64 sync_revision = 0;
        qint64 sync_dirty = 0;
    };

    ReadResult profileList(QList<ProfileConfig>* profiles) const;
    bool addProfile(ProfileConfig& profile);
    bool modifyProfile(const ProfileConfig& profile);
    bool removeProfile(qint64 profile_id);
    FindResult findProfile(qint64 profile_id, ProfileConfig* profile) const;
    FindResult findProfileByGuid(const QString& guid, ProfileConfig* profile) const;
    ProfileConfig defaultProfile() const;
    ProfileConfig hostProfile(const HostConfig& host) const;
    ReadResult unsyncedProfiles(QList<UnsyncedProfile>* profiles) const;

    bool import(const std::optional<RouterConfig>& router,
                const QList<LocalGroupConfig>& local_groups,
                const QList<LocalHostConfig>& local_hosts,
                const QList<RouterHostConfig>& router_hosts,
                const QList<CredentialConfig>& credentials,
                const QList<ProfileConfig>& profiles);

    // Settings.
    QString name() const;
    bool setName(const QString& name);

    QString displayName() const;
    bool setDisplayName(const QString& name);

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

    // Master password.
    bool isMasterPasswordSet() const;

    class Reencryption final
    {
    public:
        explicit Reencryption(Database& db);

        ReencryptResult begin();
        bool commit(const QByteArray& salt, const QByteArray& verifier, quint32 version);

    private:
        Database& db_;
        std::optional<SqlTransaction> transaction_;
        QList<LocalHostConfig> local_hosts_;
        std::optional<RouterConfig> router_;
        QList<RouterHostConfig> router_hosts_;
        QList<CredentialConfig> credentials_;
        ProxyConfig proxy_;

        Q_DISABLE_COPY_MOVE(Reencryption)
    };

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

    static void setDirectoryPathForTesting(const QString& dir_path);
    static bool createTables(SqlDatabase& db);

    bool openDatabase();
    bool setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version);
    bool rewriteEncryptedData(const QList<LocalHostConfig>& local_hosts,
                              const std::optional<RouterConfig>& router,
                              const QList<RouterHostConfig>& router_hosts,
                              const QList<CredentialConfig>& credentials,
                              const ProxyConfig& proxy);

    QString readSetting(const QString& name, bool* found = nullptr) const;
    bool writeSetting(const QString& name, const QString& value);
    bool ensureDefaultProfile();
    bool writeRouter(const RouterConfig& router);
    bool clearRouter();
    bool resetSyncState();
    bool discardSyncData();
    bool setReceivedRevision(proto::router::SyncDataType type, qint64 revision);
    bool clearReceivedRevisions();

    mutable SqlDatabase db_;
};

#endif // CLIENT_STORAGE_DATABASE_H
