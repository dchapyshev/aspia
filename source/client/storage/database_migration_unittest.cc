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

#include "client/storage/database_migration.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/database.h"
#include "client/storage/user_settings.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

// The shapes of the address book the migration meets: the oldest release it takes, the last release
// with several routers, and the builds after it with connection profiles.
enum class Schema { V3_0_15, V3_3_4, DEV };

const char kFirstGuid[] = "11111111-1111-4111-8111-111111111111";
const char kSecondGuid[] = "22222222-2222-4222-8222-222222222222";

//--------------------------------------------------------------------------------------------------
QByteArray seal(const google::protobuf::MessageLite& message, const QByteArray& aad)
{
    std::optional<QByteArray> sealed = DataCryptor::instance().encrypt(serialize(message), aad);
    EXPECT_TRUE(sealed.has_value());
    return sealed.value_or(QByteArray());
}

//--------------------------------------------------------------------------------------------------
QByteArray fileHash(const QString& file_path)
{
    QFile file(file_path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

//--------------------------------------------------------------------------------------------------
QSet<QString> hostNames(const Database& db)
{
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(db.allLocalHosts(&hosts), Database::ReadResult::OK);

    QSet<QString> names;
    for (const LocalHostConfig& host : std::as_const(hosts))
        names.insert(host.name());

    return names;
}

//--------------------------------------------------------------------------------------------------
std::optional<LocalHostConfig> hostByName(const Database& db, const QString& name)
{
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(db.allLocalHosts(&hosts), Database::ReadResult::OK);

    for (const LocalHostConfig& host : std::as_const(hosts))
    {
        if (host.name() == name)
            return host;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
class DatabaseMigrationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // The records of the old file and of the new ones open with the key of the master password.
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        old_path_ = dir_.filePath("client.db3");
        dir_path_ = dir_.filePath("client");
    }

    QString firstPath() const { return dir_path_ + '/' + kFirstGuid + ".db3"; }

    // The tables of the address book the way a release created them.
    void createOldBook(Schema schema)
    {
        ASSERT_TRUE(old_.open(old_path_));

        const QString os = (schema != Schema::V3_0_15) ? ", os_credential_id INTEGER" : "";
        const QString profile = (schema == Schema::DEV) ? ", profile_id INTEGER" : "";

        exec("CREATE TABLE local_groups (id INTEGER UNIQUE, parent_id INTEGER, name TEXT NOT NULL DEFAULT '', "
             "comment TEXT NOT NULL DEFAULT '', guid TEXT NOT NULL UNIQUE, PRIMARY KEY(id AUTOINCREMENT))");
        exec(QString("CREATE TABLE local_hosts (id INTEGER UNIQUE, group_id INTEGER, "
                     "router_id INTEGER NOT NULL DEFAULT 0, name TEXT NOT NULL DEFAULT '', "
                     "comment TEXT NOT NULL DEFAULT '', data BLOB DEFAULT X'', "
                     "create_time INTEGER NOT NULL DEFAULT 0, modify_time INTEGER NOT NULL DEFAULT 0, "
                     "connect_time INTEGER NOT NULL DEFAULT 0, guid TEXT NOT NULL UNIQUE, "
                     "credential_id INTEGER%1%2, PRIMARY KEY(id AUTOINCREMENT))").arg(os, profile));
        exec("CREATE TABLE routers (id INTEGER UNIQUE, name TEXT NOT NULL DEFAULT '', "
             "session_type INTEGER NOT NULL DEFAULT 0, data BLOB DEFAULT X'', guid TEXT NOT NULL UNIQUE, "
             "PRIMARY KEY(id AUTOINCREMENT))");
        exec(QString("CREATE TABLE router_hosts (router_id INTEGER NOT NULL, host_id INTEGER NOT NULL, "
                     "check_time INTEGER NOT NULL DEFAULT 0, data BLOB DEFAULT X'', credential_id INTEGER%1%2, "
                     "PRIMARY KEY(router_id, host_id))").arg(os, profile));
        exec("CREATE TABLE credentials (id INTEGER UNIQUE, type INTEGER NOT NULL DEFAULT 0, "
             "name TEXT NOT NULL DEFAULT '', data BLOB DEFAULT X'', guid TEXT NOT NULL UNIQUE, "
             "PRIMARY KEY(id AUTOINCREMENT))");
        exec("CREATE TABLE settings (name TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)");

        if (schema == Schema::DEV)
        {
            exec("CREATE TABLE profiles (id INTEGER UNIQUE, name TEXT NOT NULL DEFAULT '', data BLOB DEFAULT X'', "
                 "guid TEXT NOT NULL UNIQUE, PRIMARY KEY(id AUTOINCREMENT))");
        }
    }

    void exec(const QString& sql)
    {
        const QByteArray text = sql.toUtf8();
        ASSERT_TRUE(old_.exec(text.constData())) << old_.lastError().toStdString();
    }

    void addOldRouter(qint64 id, const QString& name, const QString& guid,
                      proto::router::SessionType session_type = proto::router::SESSION_TYPE_OPERATOR)
    {
        // The record before the name and the type of the session moved into it.
        proto::storage::RouterBlob data;
        data.set_address("router.example.com");
        data.set_username("router-user");
        data.set_password("router-secret");

        SqlQuery query(old_, "INSERT INTO routers (id, name, session_type, data, guid) VALUES (?, ?, ?, ?, ?)");
        query.addInt64(id);
        query.addText(name);
        query.addInt64(session_type);
        query.addBlob(seal(data, "routers"));
        query.addText(guid);
        ASSERT_TRUE(query.exec());
    }

    void addOldGroup(qint64 id, const QString& name)
    {
        SqlQuery query(old_, "INSERT INTO local_groups (id, name, guid) VALUES (?, ?, ?)");
        query.addInt64(id);
        query.addText(name);
        query.addText(QUuid::createUuid().toString(QUuid::WithoutBraces));
        ASSERT_TRUE(query.exec());
    }

    void addOldHost(qint64 id, qint64 router_id, const QString& name, const QString& address,
                    qint64 group_id = 0, qint64 credential_id = 0)
    {
        LocalHostConfig host;
        host.setAddress(address);

        std::optional<QByteArray> data = host.encryptedData();
        ASSERT_TRUE(data.has_value());

        SqlQuery query(old_, "INSERT INTO local_hosts (id, group_id, router_id, name, data, guid, credential_id) "
                             "VALUES (?, NULLIF(?, 0), ?, ?, ?, ?, NULLIF(?, 0))");
        query.addInt64(id);
        query.addInt64(group_id);
        query.addInt64(router_id);
        query.addText(name);
        query.addBlob(*data);
        query.addText(QUuid::createUuid().toString(QUuid::WithoutBraces));
        query.addInt64(credential_id);
        ASSERT_TRUE(query.exec());
    }

    void addOldRouterHost(qint64 router_id, HostId host_id, const QString& username, const QString& password,
                          qint64 credential_id = 0)
    {
        proto::storage::HostBlob data;
        data.set_username(username.toStdString());
        data.set_password(password.toStdString());

        // The row is sealed for the router it belongs to as well.
        const QByteArray aad = QByteArrayLiteral("router_hosts/") + QByteArray::number(router_id) + '/' +
                               QByteArray::number(host_id);

        SqlQuery query(old_, "INSERT INTO router_hosts (router_id, host_id, data, credential_id) "
                             "VALUES (?, ?, ?, NULLIF(?, 0))");
        query.addInt64(router_id);
        query.addUInt64(host_id);
        query.addBlob(seal(data, aad));
        query.addInt64(credential_id);
        ASSERT_TRUE(query.exec());
    }

    void addOldCredential(qint64 id, const QString& username, const QString& password)
    {
        CredentialConfig credential;
        credential.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
        credential.setUsername(username);
        credential.setPassword(SecureString(password));

        std::optional<QByteArray> data = credential.encryptedData();
        ASSERT_TRUE(data.has_value());

        SqlQuery query(old_, "INSERT INTO credentials (id, name, data, guid) VALUES (?, ?, ?, ?)");
        query.addInt64(id);
        query.addText(QString("credential"));
        query.addBlob(*data);
        query.addText(credential.guid());
        ASSERT_TRUE(query.exec());
    }

    void addOldSetting(const QString& name, const QString& value)
    {
        SqlQuery query(old_, "INSERT INTO settings (name, value) VALUES (?, ?)");
        query.addText(name);
        query.addText(value);
        ASSERT_TRUE(query.exec());
    }

    void closeOldBook() { old_.close(); }

    // An address book of |schema| with two routers and a host of each kind, moved whole: the
    // database of the first router is written at the start and its router once it is unlocked.
    void migrateBookOf(Schema schema)
    {
        createOldBook(schema);
        addOldRouter(1, "office", kFirstGuid, proto::router::SESSION_TYPE_ADMIN);
        addOldRouter(2, "branch", kSecondGuid);
        addOldGroup(1, "group");
        addOldCredential(3, "shared-user", "shared-secret");
        addOldHost(1, 0, "direct", "192.168.0.1", 1, 3);
        addOldHost(2, 1, "office-host", "100500", 1);
        addOldRouterHost(1, 100500, "user", "secret", 3);
        addOldRouterHost(2, 100600, "other-user", "other-secret");

        if (schema != Schema::V3_0_15)
            exec("UPDATE router_hosts SET os_credential_id=3 WHERE host_id=100500");

        if (schema == Schema::DEV)
        {
            exec("INSERT INTO profiles (id, name, guid) VALUES (5, 'fast', '33333333-3333-4333-8333-333333333333')");
            exec("UPDATE local_hosts SET profile_id=5 WHERE id=1");
            exec("UPDATE router_hosts SET profile_id=5 WHERE host_id=100500");
        }

        closeOldBook();

        ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

        Database db;
        ASSERT_TRUE(db.open(firstPath()));
        ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));

        EXPECT_EQ(db.guid(), QString(kFirstGuid));
        EXPECT_EQ(hostNames(db), QSet<QString>({ "direct", "office-host" }));

        const std::optional<LocalHostConfig> direct = hostByName(db, "direct");
        ASSERT_TRUE(direct.has_value());
        EXPECT_EQ(direct->address(), QString("192.168.0.1"));
        EXPECT_EQ(direct->credentialId(), 3);
        EXPECT_EQ(direct->profileId(), schema == Schema::DEV ? 5 : 0);

        QList<LocalGroupConfig> groups;
        ASSERT_TRUE(db.allLocalGroups(&groups));
        ASSERT_EQ(groups.size(), 1);
        EXPECT_EQ(direct->groupId(), groups.front().id());

        CredentialConfig credential;
        ASSERT_EQ(db.findCredential(3, &credential), Database::FindResult::FOUND);
        EXPECT_EQ(credential.username(), QString("shared-user"));

        RouterConfig router;
        ASSERT_EQ(db.routerConfig(&router), Database::FindResult::FOUND);
        EXPECT_EQ(router.address(), QString("router.example.com"));
        EXPECT_EQ(router.username(), QString("router-user"));
        EXPECT_EQ(router.password().toString(), QString("router-secret"));
        EXPECT_EQ(router.sessionType(), proto::router::SESSION_TYPE_ADMIN);
        EXPECT_TRUE(router.isEnabled());

        RouterHostConfig router_host;
        ASSERT_EQ(db.findRouterHost(100500, &router_host), Database::FindResult::FOUND);
        EXPECT_EQ(router_host.username(), QString("user"));
        EXPECT_EQ(router_host.password().toString(), QString("secret"));
        EXPECT_EQ(router_host.credentialId(), 3);
        EXPECT_EQ(router_host.osCredentialId(), schema != Schema::V3_0_15 ? 3 : 0);
        EXPECT_EQ(router_host.profileId(), schema == Schema::DEV ? 5 : 0);

        // The credentials of a host of the other router are of its own database.
        EXPECT_EQ(db.findRouterHost(100600, &router_host), Database::FindResult::NOT_FOUND);
    }

    QTemporaryDir dir_;
    QString old_path_;
    QString dir_path_;
    SqlDatabase old_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, NothingIsDoneWithoutTheOldFile)
{
    EXPECT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::NOT_NEEDED);
    EXPECT_FALSE(QFileInfo::exists(dir_path_));
}

//--------------------------------------------------------------------------------------------------
// The databases are already there, whatever the old file holds.
TEST_F(DatabaseMigrationTest, ExistingDirectoryIsLeftAlone)
{
    createOldBook(Schema::DEV);
    addOldRouter(1, "office", kFirstGuid);
    closeOldBook();

    ASSERT_TRUE(QDir().mkpath(dir_path_));

    EXPECT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::NOT_NEEDED);
    EXPECT_TRUE(QDir(dir_path_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, BookOfTheOldestReleaseIsMigrated)
{
    migrateBookOf(Schema::V3_0_15);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, BookOfTheLastReleaseWithSeveralRoutersIsMigrated)
{
    migrateBookOf(Schema::V3_3_4);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, BookWithProfilesIsMigrated)
{
    migrateBookOf(Schema::DEV);
}

//--------------------------------------------------------------------------------------------------
// Every router takes a database named by its guid. A direct host is in each of them, a host of a
// router in the database of its router only, and a host of a router that is gone in none.
TEST_F(DatabaseMigrationTest, EveryRouterGetsADatabaseOfItsOwn)
{
    createOldBook(Schema::DEV);
    addOldRouter(2, "second", kSecondGuid, proto::router::SESSION_TYPE_ADMIN);
    addOldRouter(1, "first", kFirstGuid);
    addOldGroup(1, "group");
    addOldHost(1, 0, "direct", "192.168.0.1");
    addOldHost(2, 1, "first-host", "100500");
    addOldHost(3, 2, "second-host", "100501");
    addOldHost(4, 7, "orphan", "100502");
    addOldSetting("display_name", "operator");
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    EXPECT_FALSE(QFileInfo::exists(dir_path_ + ".tmp"));
    EXPECT_EQ(QDir(dir_path_).entryList(QDir::Files).size(), 2);

    Database first;
    ASSERT_TRUE(first.open(firstPath()));
    EXPECT_EQ(first.guid(), QString(kFirstGuid));
    EXPECT_EQ(first.displayName(), QString("operator"));
    EXPECT_EQ(hostNames(first), QSet<QString>({ "direct", "first-host" }));

    Database second;
    ASSERT_TRUE(second.open(dir_path_ + '/' + kSecondGuid + ".db3"));
    EXPECT_EQ(second.guid(), QString(kSecondGuid));
    EXPECT_EQ(second.displayName(), QString("operator"));
    EXPECT_EQ(hostNames(second), QSet<QString>({ "direct", "second-host" }));

    QList<LocalGroupConfig> groups;
    ASSERT_TRUE(second.allLocalGroups(&groups));
    EXPECT_EQ(groups.size(), 1);

    ASSERT_TRUE(DatabaseMigration::finish(second, old_path_));

    RouterConfig router;
    ASSERT_EQ(second.routerConfig(&router), Database::FindResult::FOUND);
    EXPECT_EQ(router.sessionType(), proto::router::SESSION_TYPE_ADMIN);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, DatabasesAreNumberedInTheOrderOfTheirRouters)
{
    createOldBook(Schema::V3_3_4);
    addOldRouter(2, "second", kSecondGuid);
    addOldRouter(1, "first", kFirstGuid);
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    Database first;
    ASSERT_TRUE(first.open(firstPath()));
    EXPECT_EQ(first.name(), QString("Database #1"));

    Database second;
    ASSERT_TRUE(second.open(dir_path_ + '/' + kSecondGuid + ".db3"));
    EXPECT_EQ(second.name(), QString("Database #2"));
}

//--------------------------------------------------------------------------------------------------
// The database of the first router is the one a start without a choice opens.
TEST_F(DatabaseMigrationTest, DatabaseOfTheFirstRouterIsTheLastOpened)
{
    createOldBook(Schema::V3_3_4);
    addOldRouter(2, "second", kFirstGuid);
    addOldRouter(1, "first", kSecondGuid);
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    EXPECT_EQ(UserSettings().lastDatabase(), QString(kSecondGuid));
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, UpdateSettingsAreMovedToTheUser)
{
    createOldBook(Schema::V3_3_4);
    addOldRouter(1, "first", kFirstGuid);
    addOldSetting("check_updates", "0");
    addOldSetting("update_channel", "beta");
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    UserSettings settings;
    EXPECT_FALSE(settings.isCheckUpdatesEnabled());
    EXPECT_EQ(settings.updateChannel(), QString("beta"));

    SqlDatabase db;
    ASSERT_TRUE(db.open(firstPath(), SqlDatabase::OpenMode::READ_ONLY));
    SqlQuery query(db, "SELECT name FROM settings WHERE name IN ('check_updates', 'update_channel')");
    EXPECT_EQ(query.next(), SqlQuery::StepResult::DONE);
}

//--------------------------------------------------------------------------------------------------
// The router is written once. A database that got it no longer waits, and a start after that leaves
// whatever the user changed alone.
TEST_F(DatabaseMigrationTest, RouterIsMovedOnce)
{
    createOldBook(Schema::DEV);
    addOldRouter(1, "office", kFirstGuid);
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    Database db;
    ASSERT_TRUE(db.open(firstPath()));

    RouterConfig router;
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::NOT_FOUND);

    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));
    ASSERT_EQ(db.routerConfig(&router), Database::FindResult::FOUND);

    router.setSessionType(proto::router::SESSION_TYPE_ADMIN);
    ASSERT_TRUE(db.setRouterConfig(router));

    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));
    ASSERT_EQ(db.routerConfig(&router), Database::FindResult::FOUND);
    EXPECT_EQ(router.sessionType(), proto::router::SESSION_TYPE_ADMIN);
}

//--------------------------------------------------------------------------------------------------
// Without the old file there is nothing to take the router from, so the database waits for it.
TEST_F(DatabaseMigrationTest, RouterWaitsForTheOldFile)
{
    createOldBook(Schema::DEV);
    addOldRouter(1, "office", kFirstGuid);
    addOldRouterHost(1, 100500, "user", "secret");
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    Database db;
    ASSERT_TRUE(db.open(firstPath()));

    const QString away_path = old_path_ + ".away";
    ASSERT_TRUE(QFile::rename(old_path_, away_path));

    EXPECT_FALSE(DatabaseMigration::finish(db, old_path_));

    RouterConfig router;
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::NOT_FOUND);

    ASSERT_TRUE(QFile::rename(away_path, old_path_));

    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::FOUND);

    RouterHostConfig router_host;
    EXPECT_EQ(db.findRouterHost(100500, &router_host), Database::FindResult::FOUND);
}

//--------------------------------------------------------------------------------------------------
// A router whose record does not open has nothing to be written with. The credentials of its hosts
// open all the same, and the database does not wait for the router any longer.
TEST_F(DatabaseMigrationTest, RouterThatDoesNotOpenIsNotMoved)
{
    createOldBook(Schema::DEV);
    addOldRouter(1, "office", kFirstGuid);
    addOldRouterHost(1, 100500, "user", "secret");
    exec("UPDATE routers SET data=X'00' WHERE id=1");
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    Database db;
    ASSERT_TRUE(db.open(firstPath()));
    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));

    RouterConfig router;
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::NOT_FOUND);

    RouterHostConfig router_host;
    EXPECT_EQ(db.findRouterHost(100500, &router_host), Database::FindResult::FOUND);

    // Mended in the old file afterwards, the router is not taken again.
    ASSERT_TRUE(old_.open(old_path_));
    exec("DELETE FROM routers");
    addOldRouter(1, "office", kFirstGuid);
    closeOldBook();

    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// Without routers there is one database, and the hosts of routers that are gone have nowhere to go.
TEST_F(DatabaseMigrationTest, BookWithoutRoutersGivesOneDatabase)
{
    createOldBook(Schema::V3_3_4);
    addOldHost(1, 0, "direct", "192.168.0.1");
    addOldHost(2, 4, "orphan", "100500");
    closeOldBook();

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);
    const QStringList file_names = QDir(dir_path_).entryList(QDir::Files);
    ASSERT_EQ(file_names.size(), 1);

    Database db;
    ASSERT_TRUE(db.open(dir_path_ + '/' + file_names.front()));
    ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));

    EXPECT_FALSE(db.guid().isEmpty());
    EXPECT_EQ(db.name(), QString("Database #1"));
    EXPECT_EQ(hostNames(db), QSet<QString>({ "direct" }));

    RouterConfig router;
    EXPECT_EQ(db.routerConfig(&router), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseMigrationTest, OldFileIsOnlyRead)
{
    createOldBook(Schema::DEV);
    addOldRouter(1, "office", kFirstGuid);
    addOldRouter(2, "branch", kSecondGuid);
    addOldHost(1, 1, "office-host", "100500");
    addOldRouterHost(1, 100500, "user", "secret");
    closeOldBook();

    const QByteArray before = fileHash(old_path_);

    ASSERT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::DONE);

    {
        Database db;
        ASSERT_TRUE(db.open(firstPath()));
        ASSERT_TRUE(DatabaseMigration::finish(db, old_path_));
    }

    EXPECT_EQ(fileHash(old_path_), before);
}

//--------------------------------------------------------------------------------------------------
// A file that does not read leaves no databases behind, so the next start tries again.
TEST_F(DatabaseMigrationTest, FailureLeavesNothingBehind)
{
    QFile file(old_path_);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write(QByteArray(4096, 'x')), 0);
    file.close();

    EXPECT_EQ(DatabaseMigration::migrate(old_path_, dir_path_), DatabaseMigration::Result::FAILED);
    EXPECT_FALSE(QFileInfo::exists(dir_path_));
    EXPECT_FALSE(QFileInfo::exists(dir_path_ + ".tmp"));
}
