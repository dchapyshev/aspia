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

#include "base/sql/sql_database.h"
#include "base/sql/sql_transaction.h"
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

    bool import(const std::optional<RouterConfig>& router,
                const QList<LocalGroupConfig>& local_groups,
                const QList<LocalHostConfig>& local_hosts,
                const QList<RouterHostConfig>& router_hosts,
                const QList<CredentialConfig>& credentials,
                const QList<ProfileConfig>& profiles);

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

private:
    friend class DatabaseMigration;
    friend class DatabaseStore;
    friend class DatabaseTestPeer;

    static void setDirectoryPathForTesting(const QString& dir_path);
    static bool createTables(SqlDatabase& db);

    bool openDatabase();
    bool rewriteEncryptedData(const QList<LocalHostConfig>& local_hosts,
                              const std::optional<RouterConfig>& router,
                              const QList<RouterHostConfig>& router_hosts,
                              const QList<CredentialConfig>& credentials,
                              const ProxyConfig& proxy);

    mutable SqlDatabase db_;
};

#endif // CLIENT_STORAGE_DATABASE_H
