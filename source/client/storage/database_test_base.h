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

#ifndef CLIENT_STORAGE_DATABASE_TEST_BASE_H
#define CLIENT_STORAGE_DATABASE_TEST_BASE_H

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/sql/sql_query.h"
#include "client/storage/credential_store.h"

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<RouterConfig> findRouter(const Database& db)
{
    RouterConfig router;
    const Database::FindResult result = db.routerConfig(&router);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return router;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<CredentialConfig> findCredential(Database& db, qint64 credential_id)
{
    CredentialConfig credential;
    const Database::FindResult result = CredentialStore(db).find(credential_id, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<CredentialConfig> findCredentialByGuid(Database& db, const QString& guid)
{
    CredentialConfig credential;
    const Database::FindResult result = CredentialStore(db).findByGuid(guid, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
inline Database::ReencryptResult reencrypt(
    Database& db, const SecureByteArray& key, const QByteArray& salt, const QByteArray& verifier)
{
    Database::Reencryption reencryption(db);

    const Database::ReencryptResult begun = reencryption.begin();
    if (begun != Database::ReencryptResult::DONE)
        return begun;

    DataCryptor::instance().setKey(key);

    if (!reencryption.commit(salt, verifier, 1))
        return Database::ReencryptResult::FAILED;

    return Database::ReencryptResult::DONE;
}

class DatabaseTestBase : public testing::Test
{
protected:
    void SetUp() override
    {
        // A record of the address book keeps its text encrypted with a key of the process, which
        // the application sets once the master password is entered.
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        file_path_ = dir_.filePath("client.db3");
        ASSERT_TRUE(db_.open(file_path_));
    }

    // The moment the row |id| of |table| was last edited, which the public API hands out for the
    // local hosts only.
    qint64 modifyTime(const QString& table, const QString& key, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return -1;

        const std::string sql =
            QString("SELECT modify_time FROM %1 WHERE %2=?").arg(table, key).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        if (query.next() != SqlQuery::StepResult::ROW)
            return -1;

        return query.columnInt64(0);
    }

    // Writes a moment the public API cannot produce, since it always stamps the current one.
    bool setModifyTime(const QString& table, const QString& key, qint64 id, qint64 modify_time)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET modify_time=? WHERE %2=?").arg(table, key).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(modify_time);
        query.addInt64(id);

        return query.exec();
    }

    // Marks the row |id| of |table| as one the router holds at |revision| with no changes since,
    // which only an exchange with the router does.
    bool markSynced(const QString& table, qint64 id, qint64 revision)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET sync_revision=?, sync_dirty=0 WHERE id=?").arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(revision);
        query.addInt64(id);

        return query.exec();
    }

    // The rows of |table|, the ones marked as deleted, which the public API skips, included.
    qint64 rowCount(const QString& table)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return -1;

        const std::string sql = QString("SELECT COUNT(*) FROM %1").arg(table).toStdString();

        SqlQuery query(raw, sql);
        if (query.next() != SqlQuery::StepResult::ROW)
            return -1;

        return query.columnInt64(0);
    }

    // True when the row |id| of |table| is in place, marked as deleted, with its name and its data
    // wiped.
    bool isMarkedDeleted(const QString& table, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql = QString("SELECT COUNT(*) FROM %1 WHERE id=? AND deleted=1 AND name='' AND data=X''")
                                    .arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        return query.next() == SqlQuery::StepResult::ROW && query.columnInt64(0) == 1;
    }

    // Spoils the sealed text of a record the way a damaged database or one of another installation
    // has it: the row is in place, its data does not open.
    bool corruptRecordData(const QString& table, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET data=X'00' WHERE id=?").arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        return query.exec();
    }

    void addRouter(bool sync_enabled = false)
    {
        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));
        router.setSyncEnabled(sync_enabled);

        EXPECT_TRUE(db_.setRouterConfig(router));
    }

    // The synchronization changes the records in a transaction of the storage the device is
    // attached to, so a test that plays the synchronization attaches the device first.
    void attachSyncStorage()
    {
        addRouter(true);

        const SecureByteArray key(Random::byteArray(32));
        ASSERT_EQ(db_.setSyncStorage(*findRouter(db_), Random::byteArray(16), key, false), Database::SyncResult::DONE);
    }

    QByteArray syncStorageId()
    {
        const std::optional<RouterConfig> router = findRouter(db_);
        return router.has_value() ? router->syncStorageId() : QByteArray();
    }

    CredentialConfig credential(const QString& name, const QString& username,
                                const QString& password)
    {
        CredentialConfig config;
        config.setDisplayName(name);
        config.setUsername(username);
        config.setPassword(SecureString(password));
        return config;
    }

    qint64 addCredential(const QString& name, const QString& username, const QString& password)
    {
        CredentialConfig config = credential(name, username, password);
        EXPECT_TRUE(credentials_.add(config));
        return config.id();
    }

    // The list scans of the fixture: the read is expected to succeed, and the list comes back by
    // value the way the tests consume it.
    QList<CredentialConfig> credentialList()
    {
        QList<CredentialConfig> credentials;
        EXPECT_EQ(credentials_.list(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    QList<CredentialStore::Unsynced> unsyncedCredentials()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(credentials_.unsynced(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    bool storeReceivedCredential(const CredentialConfig& credential, qint64 sync_revision)
    {
        Database::SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               credentials_.storeReceived(transaction, credential, sync_revision) && transaction.commit();
    }

    Database db_;
    CredentialStore credentials_{ db_ };
    QString file_path_;

private:
    QTemporaryDir dir_;
};

#endif // CLIENT_STORAGE_DATABASE_TEST_BASE_H
