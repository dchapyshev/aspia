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

#include "client/storage/credential_store.h"

#include <QDateTime>

#include "client/storage/database_test_base.h"

using CredentialStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// Every write of a credential stamps the moment it was edited, the first one included.
TEST_F(CredentialStoreTest, EditMarksTheMomentACredentialChanged)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();

    const qint64 credential_id = addCredential("name", "user", "secret");
    EXPECT_GE(modifyTime("credentials", "id", credential_id), start_time);

    ASSERT_TRUE(setModifyTime("credentials", "id", credential_id, 1));

    CredentialConfig edited = credential("renamed", "user", "secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));

    EXPECT_GE(modifyTime("credentials", "id", credential_id), start_time);
}

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialStoreTest, CredentialThatDoesNotOpenLeavesTheListIncomplete)
{
    const qint64 broken_id = addCredential("office", "user", "secret");
    addCredential("home", "user", "secret");

    ASSERT_TRUE(corruptRecordData("credentials", broken_id));

    // The record that did not open is in the list all the same: without it the user would have
    // no way to reach the row, and the result says the list is not whole.
    QList<CredentialConfig> credentials;
    EXPECT_EQ(credentials_.list(&credentials), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(credentials.size(), 2);

    const CredentialConfig& broken = credentials.front().id() == broken_id ?
        credentials.front() : credentials.back();
    const CredentialConfig& readable = credentials.front().id() == broken_id ?
        credentials.back() : credentials.front();

    EXPECT_EQ(broken.displayName(), QString("office"));
    EXPECT_TRUE(broken.username().isEmpty());
    EXPECT_EQ(readable.displayName(), QString("home"));
    EXPECT_EQ(readable.username(), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// Credentials come by name whatever order they were added in, and the case of a letter does not
// move them.
TEST_F(CredentialStoreTest, CredentialsAreListedByName)
{
    addCredential("office", "user", "secret");
    addCredential("Home", "user", "secret");
    addCredential(QString::fromUtf8("Склад"), "user", "secret");
    addCredential("backup", "user", "secret");

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 4);
    EXPECT_EQ(credentials[0].displayName(), "backup");
    EXPECT_EQ(credentials[1].displayName(), "Home");
    EXPECT_EQ(credentials[2].displayName(), "office");
    EXPECT_EQ(credentials[3].displayName(), QString::fromUtf8("Склад"));
}

//--------------------------------------------------------------------------------------------------
// A credential is named by its guid everywhere it is referred to, so every one gets a guid the
// moment it is added, and no two share one.
TEST_F(CredentialStoreTest, CredentialGetsAGuidOfItsOwn)
{
    const qint64 first = addCredential("first", "user", "secret");
    const qint64 second = addCredential("second", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, first);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->guid().isEmpty());

    const std::optional<CredentialConfig> other = findCredential(db_, second);
    ASSERT_TRUE(other.has_value());
    EXPECT_NE(stored->guid(), other->guid());

    CredentialConfig copy = credential("copy", "user", "secret");
    copy.setGuid(stored->guid());
    EXPECT_FALSE(credentials_.add(copy));

    EXPECT_EQ(credentialList().size(), 2);
}

//--------------------------------------------------------------------------------------------------
// The pair goes to the database as one sealed column. What comes back out has to be what went in.
TEST_F(CredentialStoreTest, CredentialSurvivesARoundTrip)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->id(), credential_id);
    EXPECT_EQ(stored->type(), CredentialConfig::Type::ASPIA);
    EXPECT_EQ(stored->displayName(), QString("office"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));

    // The guid names the same record wherever it is referred to.
    const std::optional<CredentialConfig> by_guid = findCredentialByGuid(db_, stored->guid());
    ASSERT_TRUE(by_guid.has_value());
    EXPECT_EQ(by_guid->id(), credential_id);
}

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialStoreTest, OsCredentialKeepsItsType)
{
    CredentialConfig added = credential("workstation", "DOMAIN\\user", "secret");
    added.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(credentials_.add(added));

    std::optional<CredentialConfig> stored = findCredential(db_, added.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::OS);
    EXPECT_EQ(stored->username(), QString("DOMAIN\\user"));

    CredentialConfig edited = credential("renamed", "DOMAIN\\other-user", "other-secret");
    edited.setId(added.id());
    edited.setType(CredentialConfig::Type::OS);
    EXPECT_TRUE(credentials_.modify(edited));

    stored = findCredential(db_, added.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::OS);
    EXPECT_EQ(stored->displayName(), QString("renamed"));
    EXPECT_EQ(stored->username(), QString("DOMAIN\\other-user"));

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].type(), CredentialConfig::Type::OS);
}

//--------------------------------------------------------------------------------------------------
// A list asked for one type holds only the records of that type, still by name.
TEST_F(CredentialStoreTest, CredentialsAreListedByType)
{
    addCredential("office", "user", "secret");

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(credentials_.add(os));

    addCredential("backup", "user", "secret");

    QList<CredentialConfig> credentials;
    EXPECT_EQ(credentials_.list(&credentials, CredentialConfig::Type::ASPIA), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 2);
    EXPECT_EQ(credentials[0].displayName(), "backup");
    EXPECT_EQ(credentials[1].displayName(), "office");

    EXPECT_EQ(credentials_.list(&credentials, CredentialConfig::Type::OS), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].displayName(), "workstation");

    EXPECT_EQ(credentialList().size(), 3);
}

//--------------------------------------------------------------------------------------------------
// An edit changes the name and the pair and keeps the guid, since the records referring to it would
// stop resolving otherwise. A record that was never added has no row to edit, and removing takes
// the row out.
TEST_F(CredentialStoreTest, CredentialIsEditedAndRemoved)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> before = findCredential(db_, credential_id);
    ASSERT_TRUE(before.has_value());

    // What the editor hands over: the fields of the form and the id, the guid is what the record
    // already has.
    CredentialConfig edited = credential("renamed", "other-user", "other-secret");
    edited.setId(credential_id);
    edited.setGuid(before->guid());
    EXPECT_TRUE(credentials_.modify(edited));

    CredentialConfig unknown = credential("unknown", "user", "secret");
    unknown.setId(credential_id + 1);
    EXPECT_FALSE(credentials_.modify(unknown));

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->displayName(), QString("renamed"));
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->password().toString(), QString("other-secret"));
    EXPECT_EQ(credentialList().size(), 1);

    ASSERT_TRUE(credentials_.remove(credential_id));
    EXPECT_FALSE(findCredential(db_, credential_id).has_value());
    EXPECT_TRUE(credentialList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The guid names the record from the moment it is added and is what its column is sealed for. An
// edit changes neither: whatever guid the editor hands over, or none, the column is sealed for the
// guid of the row, and the record opens after the edit as it did before.
TEST_F(CredentialStoreTest, EditedCredentialStaysSealedForItsOwnGuid)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> before = findCredential(db_, credential_id);
    ASSERT_TRUE(before.has_value());

    CredentialConfig edited = credential("renamed", "other-user", "other-secret");
    edited.setId(credential_id);
    EXPECT_TRUE(credentials_.modify(edited));

    std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->username(), QString("other-user"));

    edited.setGuid("not-the-guid-of-the-row");
    EXPECT_TRUE(credentials_.modify(edited));

    stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->username(), QString("other-user"));
}

//--------------------------------------------------------------------------------------------------
// Every column of the table opens with the same key, so a column is sealed for its guid and does
// not open under another one.
TEST_F(CredentialStoreTest, CredentialDoesNotOpenForAnotherGuid)
{
    CredentialConfig config = credential("office", "user", "secret");
    config.setGuid("first");

    std::optional<QByteArray> sealed = config.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    CredentialConfig other;
    other.setGuid("second");

    EXPECT_FALSE(other.setEncryptedData(*sealed));
    EXPECT_TRUE(other.username().isEmpty());
    EXPECT_TRUE(other.password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A credential is a name and a whole pair. A record missing any of the three is not stored.
TEST_F(CredentialStoreTest, CredentialWithoutANameOrHalfItsPairIsNotStored)
{
    CredentialConfig nameless = credential(QString(), "user", "secret");
    EXPECT_FALSE(credentials_.add(nameless));

    CredentialConfig without_password = credential("office", "user", QString());
    EXPECT_FALSE(credentials_.add(without_password));

    CredentialConfig without_user = credential("office", QString(), "secret");
    EXPECT_FALSE(credentials_.add(without_user));

    EXPECT_TRUE(credentialList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// What the router has to be told is kept with the record itself: that it has never held the
// record, and how many times the record was edited since the last exchange. A change of the master
// password is not an edit.
TEST_F(CredentialStoreTest, NewAndEditedCredentialsAreUnsynced)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    const qint64 credential_id = addCredential("office", "user", "secret");

    QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.id(), credential_id);
    EXPECT_EQ(unsynced[0].credential.username(), QString("user"));
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_GE(unsynced[0].credential.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    EXPECT_TRUE(unsyncedCredentials().isEmpty());

    ASSERT_EQ(reencrypt(db_, DataCryptor::instance().key(), "salt", "verifier"), Database::ReencryptResult::DONE);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());

    CredentialConfig edited = credential("office", "other-user", "other-secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(credentials_.modify(edited));

    unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.username(), QString("other-user"));
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 2);
}

//--------------------------------------------------------------------------------------------------
// There is nothing to tell the router of a record it has never held, so such a record is gone the
// moment it is removed.
TEST_F(CredentialStoreTest, RemovedCredentialTheRouterNeverHeldIsGoneAtOnce)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    CredentialConfig edited = credential("renamed", "user", "secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));

    ASSERT_TRUE(credentials_.remove(credential_id));

    EXPECT_EQ(rowCount("credentials"), 0);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A record received from the router is stored the way the router holds it: under its guid, with
// its revision and with nothing left to send. A newer revision replaces the record in its row, and
// the revision the row already has changes nothing.
TEST_F(CredentialStoreTest, ReceivedCredentialIsStoredWithTheRevisionOfTheRouter)
{
    attachSyncStorage();

    const QString guid = "11111111-1111-1111-1111-111111111111";

    CredentialConfig received = credential("office", "user", "secret");
    received.setGuid(guid);
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedCredential(received, 12));

    std::optional<CredentialConfig> stored = findCredentialByGuid(db_, guid);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->modifyTime(), 1000);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());

    const qint64 credential_id = stored->id();

    CredentialConfig newer = credential("renamed", "other-user", "other-secret");
    newer.setGuid(guid);
    newer.setModifyTime(2000);
    ASSERT_TRUE(storeReceivedCredential(newer, 13));

    CredentialConfig repeated = credential("repeated", "repeated-user", "repeated-secret");
    repeated.setGuid(guid);
    ASSERT_TRUE(storeReceivedCredential(repeated, 13));

    stored = findCredentialByGuid(db_, guid);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->id(), credential_id);
    EXPECT_EQ(stored->displayName(), QString("renamed"));
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->modifyTime(), 2000);
    EXPECT_EQ(rowCount("credentials"), 1);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The row of a credential is found by its guid whether or not the credential is marked as removed.
// A guid no row holds is not found.
TEST_F(CredentialStoreTest, RowIsFoundByGuidWhileTheCredentialIsMarkedAsRemoved)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");

    CredentialConfig added;
    ASSERT_EQ(credentials_.find(credential_id, &added), Database::FindResult::FOUND);

    qint64 row_id = 0;
    ASSERT_EQ(credentials_.findRowIdByGuid(added.guid(), &row_id), Database::FindResult::FOUND);
    EXPECT_EQ(row_id, credential_id);

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    ASSERT_TRUE(credentials_.remove(credential_id));
    ASSERT_TRUE(isMarkedDeleted("credentials", credential_id));

    ASSERT_EQ(credentials_.findRowIdByGuid(added.guid(), &row_id), Database::FindResult::FOUND);
    EXPECT_EQ(row_id, credential_id);

    EXPECT_EQ(credentials_.findRowIdByGuid("22222222-2222-2222-2222-222222222222", &row_id),
              Database::FindResult::NOT_FOUND);
    EXPECT_EQ(row_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The guid of a credential is found by the id of its row whether or not the credential is marked as
// removed. An id no row has is not found.
TEST_F(CredentialStoreTest, GuidIsFoundByRowIdWhileTheCredentialIsMarkedAsRemoved)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");

    CredentialConfig added;
    ASSERT_EQ(credentials_.find(credential_id, &added), Database::FindResult::FOUND);

    QString guid;
    ASSERT_EQ(credentials_.findGuidByRowId(credential_id, &guid), Database::FindResult::FOUND);
    EXPECT_EQ(guid, added.guid());

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    ASSERT_TRUE(credentials_.remove(credential_id));
    ASSERT_TRUE(isMarkedDeleted("credentials", credential_id));

    ASSERT_EQ(credentials_.findGuidByRowId(credential_id, &guid), Database::FindResult::FOUND);
    EXPECT_EQ(guid, added.guid());

    EXPECT_EQ(credentials_.findGuidByRowId(credential_id + 1, &guid), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(guid.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An empty row holds the guid of a credential the device does not have yet, for the hosts that link
// it. The row is neither listed nor sent. The credential that comes with this guid takes the row and
// keeps its id.
TEST_F(CredentialStoreTest, EmptyRowIsTakenByTheCredentialThatComesWithItsGuid)
{
    attachSyncStorage();

    const QString guid = "11111111-1111-1111-1111-111111111111";

    qint64 row_id = 0;
    ASSERT_TRUE(credentials_.addEmptyRow(guid, &row_id));
    EXPECT_GT(row_id, 0);
    EXPECT_TRUE(isMarkedDeleted("credentials", row_id));

    EXPECT_TRUE(credentialList().isEmpty());
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
    EXPECT_FALSE(findCredentialByGuid(db_, guid).has_value());

    qint64 found_id = 0;
    ASSERT_EQ(credentials_.findRowIdByGuid(guid, &found_id), Database::FindResult::FOUND);
    EXPECT_EQ(found_id, row_id);

    CredentialConfig received = credential("office", "user", "secret");
    received.setGuid(guid);
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedCredential(received, 12));

    const std::optional<CredentialConfig> stored = findCredentialByGuid(db_, guid);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->id(), row_id);
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(rowCount("credentials"), 1);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
}
