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

#include "router/storage/sync_storage.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>

#include "proto/router_constants.h"
#include "proto/router_sync.h"

namespace {

const char kFirstUserGuid[] = "11111111-1111-4111-8111-111111111111";
const char kSecondUserGuid[] = "22222222-2222-4222-8222-222222222222";

//--------------------------------------------------------------------------------------------------
proto::router::SyncStorageKey makeKey(std::string_view seed)
{
    proto::router::SyncStorageKey key;
    key.set_salt(std::string(seed) + "-salt");
    key.set_verifier(std::string(seed) + "-verifier");
    key.set_data(std::string(seed) + "-data");
    return key;
}

//--------------------------------------------------------------------------------------------------
proto::router::SyncRecord makeRecord(std::string_view id, qint64 revision, std::string_view data)
{
    proto::router::SyncRecord record;
    record.set_revision(revision);
    record.set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    record.set_id(id);
    record.set_data(data);
    return record;
}

//--------------------------------------------------------------------------------------------------
proto::router::SyncRecord makeDeletedRecord(std::string_view id, qint64 revision)
{
    proto::router::SyncRecord record;
    record.set_revision(revision);
    record.set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    record.set_id(id);
    record.set_deleted(true);
    return record;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> revisions(const proto::router::SyncRecordResult& result)
{
    QList<qint64> list;
    for (qint64 revision : result.record_revision())
        list.append(revision);
    return list;
}

//--------------------------------------------------------------------------------------------------
QList<std::string> recordIds(const proto::router::SyncRecordList& list)
{
    QList<std::string> ids;
    for (const proto::router::SyncRecord& record : list.record())
        ids.append(record.id());
    return ids;
}

//--------------------------------------------------------------------------------------------------
bool createFile(const QString& file_path)
{
    QFile file(file_path);
    return file.open(QIODevice::WriteOnly);
}

} // namespace

class SyncStorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        directory_path_ = temp_dir_.path() + "/sync";
        file_path_ = directory_path_ + '/' + kFirstUserGuid + ".db3";

        SyncStorage::setDirectoryPathForTesting(directory_path_);
        ASSERT_TRUE(storage_.open(kFirstUserGuid));
    }

    proto::router::SyncStorageInfo storageInfo()
    {
        proto::router::SyncStorageInfo result;
        storage_.readStorageInfo(&result);
        return result;
    }

    proto::router::SyncStorageInfo setKey(proto::router::SyncStorageKeyRequest::Mode mode, std::string_view storage_id,
        qint64 key_revision, const proto::router::SyncStorageKey& key)
    {
        proto::router::SyncStorageKeyRequest request;
        request.set_mode(mode);
        request.set_storage_id(storage_id);
        request.set_key_revision(key_revision);
        *request.mutable_key() = key;

        proto::router::SyncStorageInfo result;

        if (mode == proto::router::SyncStorageKeyRequest::MODE_CREATE)
            storage_.createStorage(request, &result);
        else if (mode == proto::router::SyncStorageKeyRequest::MODE_RESET)
            storage_.resetStorage(request, &result);
        else
            storage_.changeKey(request, &result);

        return result;
    }

    std::string create()
    {
        return setKey(proto::router::SyncStorageKeyRequest::MODE_CREATE, std::string_view(), 0, makeKey("first"))
            .storage_id();
    }

    proto::router::SyncRecordList records(std::string_view storage_id, qint64 revision = 0,
        qint64 count = proto::router::kMaxSyncPageSize)
    {
        proto::router::SyncRecordListRequest request;
        request.set_storage_id(storage_id);
        request.set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
        request.set_revision(revision);
        request.set_count(count);

        proto::router::SyncRecordList result;
        storage_.readRecords(request, &result);
        return result;
    }

    proto::router::SyncRecordResult write(std::string_view storage_id,
        std::initializer_list<proto::router::SyncRecord> list)
    {
        proto::router::SyncRecordRequest request;
        request.set_storage_id(storage_id);
        for (const proto::router::SyncRecord& record : list)
            *request.add_record() = record;

        proto::router::SyncRecordResult result;
        bool records_changed = false;
        storage_.storeRecords(request, &result, &records_changed);
        return result;
    }

    QStringList userGuids() const
    {
        QStringList user_guids = SyncStorage::userGuids();
        user_guids.sort();
        return user_guids;
    }

    bool execRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;
        return raw.exec(sql.toStdString().c_str());
    }

    QTemporaryDir temp_dir_;
    QString directory_path_;
    QString file_path_;
    SyncStorage storage_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, UserHasNoStorageUntilItIsCreated)
{
    EXPECT_EQ(storageInfo().error_code(), proto::router::kErrorNotFound);
    EXPECT_EQ(records("anything").error_code(), proto::router::kErrorNotFound);
    EXPECT_EQ(write("anything", { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, CreatedStorageReadsBack)
{
    const proto::router::SyncStorageInfo created =
        setKey(proto::router::SyncStorageKeyRequest::MODE_CREATE, std::string_view(), 0, makeKey("first"));
    ASSERT_EQ(created.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(created.storage_id().size(), static_cast<size_t>(proto::router::kSyncStorageIdSize));
    EXPECT_EQ(created.revision(), 0);
    EXPECT_EQ(created.key_revision(), 1);
    EXPECT_EQ(created.key().salt(), "first-salt");
    EXPECT_EQ(created.key().verifier(), "first-verifier");
    EXPECT_EQ(created.key().data(), "first-data");

    const proto::router::SyncStorageInfo read = storageInfo();
    ASSERT_EQ(read.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(read.storage_id(), created.storage_id());
    EXPECT_EQ(read.key_revision(), 1);
    EXPECT_EQ(read.key().data(), "first-data");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, SecondCreateIsRefused)
{
    const std::string storage_id = create();

    const proto::router::SyncStorageInfo second =
        setKey(proto::router::SyncStorageKeyRequest::MODE_CREATE, std::string_view(), 0, makeKey("second"));
    EXPECT_EQ(second.error_code(), proto::router::kErrorAlreadyExists);

    const proto::router::SyncStorageInfo read = storageInfo();
    EXPECT_EQ(read.storage_id(), storage_id);
    EXPECT_EQ(read.key().data(), "first-data");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, KeyIsReplacedOnlyFromTheRevisionTheClientSaw)
{
    const std::string storage_id = create();

    const proto::router::SyncStorageInfo modified =
        setKey(proto::router::SyncStorageKeyRequest::MODE_MODIFY, storage_id, 1, makeKey("second"));
    ASSERT_EQ(modified.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(modified.storage_id(), storage_id);
    EXPECT_EQ(modified.key_revision(), 2);
    EXPECT_EQ(modified.key().data(), "second-data");

    const proto::router::SyncStorageInfo stale =
        setKey(proto::router::SyncStorageKeyRequest::MODE_MODIFY, storage_id, 1, makeKey("third"));
    EXPECT_EQ(stale.error_code(), proto::router::kErrorConflict);

    const proto::router::SyncStorageInfo foreign =
        setKey(proto::router::SyncStorageKeyRequest::MODE_MODIFY, "another storage", 2, makeKey("third"));
    EXPECT_EQ(foreign.error_code(), proto::router::kErrorNotFound);

    EXPECT_EQ(storageInfo().key().data(), "second-data");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, ResetDropsTheRecordsAndChangesTheStorageId)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncStorageInfo foreign =
        setKey(proto::router::SyncStorageKeyRequest::MODE_RESET, "another storage", 0, makeKey("second"));
    EXPECT_EQ(foreign.error_code(), proto::router::kErrorNotFound);
    EXPECT_EQ(records(storage_id).record_size(), 1);

    const proto::router::SyncStorageInfo reset =
        setKey(proto::router::SyncStorageKeyRequest::MODE_RESET, storage_id, 0, makeKey("second"));
    ASSERT_EQ(reset.error_code(), proto::router::kErrorOk);
    EXPECT_NE(reset.storage_id(), storage_id);
    EXPECT_EQ(reset.revision(), 0);
    EXPECT_EQ(reset.key_revision(), 1);
    EXPECT_EQ(reset.key().data(), "second-data");

    EXPECT_EQ(records(storage_id).error_code(), proto::router::kErrorNotFound);

    const proto::router::SyncRecordList list = records(reset.storage_id());
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.record_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// A storage made by a reset makes the devices attached to another one give up their records, a storage made
// in place of a lost one does not. A new key leaves that as it is.
TEST_F(SyncStorageTest, StorageTellsWhetherAResetMadeIt)
{
    const std::string storage_id = create();
    EXPECT_FALSE(storageInfo().reset());

    const proto::router::SyncStorageInfo modified =
        setKey(proto::router::SyncStorageKeyRequest::MODE_MODIFY, storage_id, 1, makeKey("second"));
    ASSERT_EQ(modified.error_code(), proto::router::kErrorOk);
    EXPECT_FALSE(modified.reset());

    const proto::router::SyncStorageInfo reset =
        setKey(proto::router::SyncStorageKeyRequest::MODE_RESET, storage_id, 0, makeKey("third"));
    ASSERT_EQ(reset.error_code(), proto::router::kErrorOk);
    EXPECT_TRUE(reset.reset());

    const proto::router::SyncStorageInfo modified_again =
        setKey(proto::router::SyncStorageKeyRequest::MODE_MODIFY, reset.storage_id(), 1, makeKey("fourth"));
    ASSERT_EQ(modified_again.error_code(), proto::router::kErrorOk);
    EXPECT_TRUE(modified_again.reset());
    EXPECT_TRUE(storageInfo().reset());
}

//--------------------------------------------------------------------------------------------------
// A device that was told to reset the storage creates one when the router holds none.
TEST_F(SyncStorageTest, ResetCreatesTheStorageWhenTheUserHasNone)
{
    const proto::router::SyncStorageInfo created =
        setKey(proto::router::SyncStorageKeyRequest::MODE_RESET, "lost storage", 0, makeKey("first"));
    ASSERT_EQ(created.error_code(), proto::router::kErrorOk);
    EXPECT_NE(created.storage_id(), "lost storage");
    EXPECT_EQ(created.key_revision(), 1);
    EXPECT_TRUE(created.reset());

    const proto::router::SyncStorageInfo read = storageInfo();
    EXPECT_EQ(read.storage_id(), created.storage_id());
    EXPECT_TRUE(read.reset());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, InvalidKeyRequestIsRefused)
{
    proto::router::SyncStorageKey empty_field = makeKey("first");
    empty_field.clear_verifier();

    proto::router::SyncStorageKey large_field = makeKey("first");
    large_field.set_data(std::string(proto::router::kMaxSyncStorageKeyFieldSize + 1, 'x'));

    const proto::router::SyncStorageKeyRequest::Mode mode = proto::router::SyncStorageKeyRequest::MODE_CREATE;

    EXPECT_EQ(setKey(mode, std::string_view(), 0, empty_field).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(setKey(mode, std::string_view(), 0, large_field).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(setKey(mode, std::string_view(), -1, makeKey("first")).error_code(),
              proto::router::kErrorInvalidRequest);

    EXPECT_EQ(storageInfo().error_code(), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, NewRecordsGetTheNextRevisions)
{
    const std::string storage_id = create();

    const proto::router::SyncRecordResult result =
        write(storage_id, { makeRecord("a", 0, "1"), makeRecord("b", 0, "2") });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 1, 2 }));
    EXPECT_EQ(result.revision(), 2);
    EXPECT_EQ(storageInfo().revision(), 2);

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.record_size(), 2);
    EXPECT_EQ(list.revision(), 2);
    EXPECT_FALSE(list.has_more());

    const proto::router::SyncRecord& first = list.record(0);
    EXPECT_EQ(first.revision(), 1);
    EXPECT_EQ(first.type(), proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    EXPECT_EQ(first.id(), "a");
    EXPECT_FALSE(first.deleted());
    EXPECT_EQ(first.data(), "1");

    EXPECT_EQ(list.record(1).revision(), 2);
    EXPECT_EQ(list.record(1).id(), "b");
}

//--------------------------------------------------------------------------------------------------
// A change based on a revision other than the stored one is not written, and the rest of the
// request is.
TEST_F(SyncStorageTest, RecordIsReplacedOnlyFromTheRevisionTheChangeWasBasedOn)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordResult result = write(storage_id,
        { makeRecord("a", 0, "stale"), makeRecord("b", 0, "2") });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 0, 2 }));
    EXPECT_EQ(result.revision(), 2);

    const proto::router::SyncRecordResult replaced = write(storage_id, { makeRecord("a", 1, "3") });
    ASSERT_EQ(replaced.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(replaced), QList<qint64>({ 3 }));

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 2);
    EXPECT_EQ(recordIds(list), QList<std::string>({ "b", "a" }));
    EXPECT_EQ(list.record(1).revision(), 3);
    EXPECT_EQ(list.record(1).data(), "3");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RefusedChangesLeaveTheRevisionOfTheStorageAlone)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordResult result = write(storage_id, { makeRecord("a", 0, "stale") });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 0 }));
    EXPECT_EQ(result.revision(), 1);
    EXPECT_EQ(storageInfo().revision(), 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, DeletedRecordStaysWithoutItsData)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordResult result = write(storage_id, { makeDeletedRecord("a", 1) });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 2 }));

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).revision(), 2);
    EXPECT_TRUE(list.record(0).deleted());
    EXPECT_TRUE(list.record(0).data().empty());

    const proto::router::SyncRecordResult revived = write(storage_id, { makeRecord("a", 2, "2") });
    ASSERT_EQ(revived.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(revived), QList<qint64>({ 3 }));
    EXPECT_FALSE(records(storage_id).record(0).deleted());
}

//--------------------------------------------------------------------------------------------------
// A record made anew under the id of a removed one is a new record: the device that makes it has
// read the removal already and holds no revision to base the record on.
TEST_F(SyncStorageTest, NewRecordIsStoredOverARemovedOne)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);
    ASSERT_EQ(write(storage_id, { makeDeletedRecord("a", 1) }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordResult result = write(storage_id, { makeRecord("a", 0, "2") });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 3 }));
    EXPECT_EQ(result.revision(), 3);

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).revision(), 3);
    EXPECT_FALSE(list.record(0).deleted());
    EXPECT_EQ(list.record(0).data(), "2");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RecordCarriesTheTimeTheRouterStoredIt)
{
    const std::string storage_id = create();
    const qint64 before = secondsSinceEpoch();

    // The time a client sends is not the one that is kept.
    proto::router::SyncRecord record = makeRecord("a", 0, "1");
    record.set_time(1);
    ASSERT_EQ(write(storage_id, { record, makeRecord("b", 0, "2") }).error_code(), proto::router::kErrorOk);

    proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 2);
    EXPECT_GE(list.record(0).time(), before);
    EXPECT_EQ(list.record(1).time(), list.record(0).time());
    EXPECT_GE(list.current_time(), list.record(0).time());

    // A new version of a record, a deletion as well, takes the time it was stored at. The records
    // left alone keep theirs.
    ASSERT_TRUE(execRaw("UPDATE records SET time=1"));
    ASSERT_EQ(write(storage_id, { makeDeletedRecord("a", 1) }).error_code(), proto::router::kErrorOk);

    list = records(storage_id);
    ASSERT_EQ(list.record_size(), 2);
    EXPECT_EQ(list.record(0).id(), "b");
    EXPECT_EQ(list.record(0).time(), 1);
    EXPECT_EQ(list.record(1).id(), "a");
    EXPECT_TRUE(list.record(1).deleted());
    EXPECT_GE(list.record(1).time(), before);

    // An answer without records tells the time of the router too.
    EXPECT_GE(records(storage_id, 3).current_time(), before);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, ListReturnsTheRecordsNewerThanTheRevision)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1"), makeRecord("b", 0, "2"), makeRecord("c", 0, "3") })
                  .error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordList tail = records(storage_id, 1);
    ASSERT_EQ(tail.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(recordIds(tail), QList<std::string>({ "b", "c" }));
    EXPECT_FALSE(tail.has_more());

    const proto::router::SyncRecordList nothing = records(storage_id, 3);
    ASSERT_EQ(nothing.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(nothing.record_size(), 0);
    EXPECT_EQ(nothing.revision(), 3);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, ListIsReadPageByPage)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1"), makeRecord("b", 0, "2"), makeRecord("c", 0, "3") })
                  .error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordList first = records(storage_id, 0, 2);
    ASSERT_EQ(first.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(recordIds(first), QList<std::string>({ "a", "b" }));
    EXPECT_TRUE(first.has_more());

    const proto::router::SyncRecordList second = records(storage_id, first.record(1).revision(), 2);
    ASSERT_EQ(second.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(recordIds(second), QList<std::string>({ "c" }));
    EXPECT_FALSE(second.has_more());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, ListReturnsOnlyTheRequestedType)
{
    const std::string storage_id = create();

    proto::router::SyncRecord other = makeRecord("a", 0, "other");
    other.set_type(static_cast<proto::router::SyncDataType>(2));

    const proto::router::SyncRecordResult result = write(storage_id, { other, makeRecord("a", 0, "1") });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 1, 2 }));

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).revision(), 2);
    EXPECT_EQ(list.record(0).data(), "1");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, InvalidListRequestIsRefused)
{
    const std::string storage_id = create();

    EXPECT_EQ(records(storage_id, -1).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(records(storage_id, 0, 0).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(records(storage_id, 0, proto::router::kMaxSyncPageSize + 1).error_code(),
              proto::router::kErrorInvalidRequest);

    proto::router::SyncRecordListRequest request;
    request.set_storage_id(storage_id);
    request.set_count(1);

    proto::router::SyncRecordList list;
    storage_.readRecords(request, &list);
    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
}

//--------------------------------------------------------------------------------------------------
// One invalid record refuses the whole request.
TEST_F(SyncStorageTest, InvalidRecordsAreRefused)
{
    const std::string storage_id = create();

    proto::router::SyncRecord unknown_type = makeRecord("a", 0, "1");
    unknown_type.set_type(proto::router::SYNC_DATA_TYPE_UNKNOWN);

    proto::router::SyncRecord deleted_with_data = makeDeletedRecord("a", 0);
    deleted_with_data.set_data("1");

    const proto::router::SyncRecord invalid[] =
    {
        unknown_type,
        deleted_with_data,
        makeRecord("a", -1, "1"),
        makeRecord("", 0, "1"),
        makeRecord(std::string(proto::router::kMaxSyncRecordIdLength + 1, 'x'), 0, "1"),
        makeRecord("a", 0, ""),
        makeRecord("a", 0, std::string(proto::router::kMaxSyncRecordDataSize + 1, 'x'))
    };

    for (const proto::router::SyncRecord& record : invalid)
    {
        const proto::router::SyncRecordResult result = write(storage_id, { makeRecord("valid", 0, "1"), record });
        EXPECT_EQ(result.error_code(), proto::router::kErrorInvalidRequest);
        EXPECT_EQ(result.record_revision_size(), 0);
    }

    EXPECT_EQ(write(storage_id, {}).error_code(), proto::router::kErrorInvalidRequest);

    proto::router::SyncRecordRequest large;
    large.set_storage_id(storage_id);
    for (int i = 0; i <= proto::router::kMaxSyncPageSize; ++i)
        *large.add_record() = makeRecord("id" + std::to_string(i), 0, "1");

    proto::router::SyncRecordResult result;
    bool records_changed = true;
    storage_.storeRecords(large, &result, &records_changed);
    EXPECT_EQ(result.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_FALSE(records_changed);

    EXPECT_EQ(records(storage_id).record_size(), 0);
    EXPECT_EQ(storageInfo().revision(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RecordsOfTheBoundSizesAreStored)
{
    const std::string storage_id = create();

    const std::string id(proto::router::kMaxSyncRecordIdLength, 'x');
    const std::string data(proto::router::kMaxSyncRecordDataSize, 'y');

    ASSERT_EQ(write(storage_id, { makeRecord(id, 0, data) }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).id(), id);
    EXPECT_EQ(list.record(0).data(), data);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RequestsForAnotherStorageAreRefused)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    EXPECT_EQ(records("another storage").error_code(), proto::router::kErrorNotFound);
    EXPECT_EQ(write("another storage", { makeRecord("b", 0, "2") }).error_code(), proto::router::kErrorNotFound);
    EXPECT_EQ(records(storage_id).record_size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A request that would take the storage over the bound stores nothing. The records the storage
// already has can still be changed.
TEST_F(SyncStorageTest, FullStorageTakesNoNewRecords)
{
    const std::string storage_id = create();
    const int last = proto::router::kMaxSyncRecordCount - 1;

    ASSERT_TRUE(execRaw(QString(
        "WITH RECURSIVE numbers(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM numbers WHERE n < %1) "
        "INSERT INTO records (type, id, revision, time, deleted, data) "
        "SELECT 1, 'id' || n, n, 0, 0, X'31' FROM numbers; "
        "UPDATE storage SET revision = %1;").arg(last)));

    const proto::router::SyncRecordResult refused =
        write(storage_id, { makeRecord("new1", 0, "1"), makeRecord("new2", 0, "2") });
    EXPECT_EQ(refused.error_code(), proto::router::kErrorLimitExceeded);
    EXPECT_EQ(refused.record_revision_size(), 0);
    EXPECT_EQ(storageInfo().revision(), last);

    const proto::router::SyncRecordResult stored = write(storage_id, { makeRecord("new1", 0, "1") });
    ASSERT_EQ(stored.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(stored), QList<qint64>({ last + 1 }));

    EXPECT_EQ(write(storage_id, { makeRecord("new2", 0, "2") }).error_code(), proto::router::kErrorLimitExceeded);

    const proto::router::SyncRecordResult replaced = write(storage_id, { makeRecord("id1", 1, "changed") });
    ASSERT_EQ(replaced.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(replaced), QList<qint64>({ last + 2 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RemovedRecordsMakeRoomInAFullStorage)
{
    const std::string storage_id = create();
    const int count = proto::router::kMaxSyncRecordCount;

    ASSERT_TRUE(execRaw(QString(
        "WITH RECURSIVE numbers(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM numbers WHERE n < %1) "
        "INSERT INTO records (type, id, revision, time, deleted, data) "
        "SELECT 1, 'id' || n, n, 0, 0, X'31' FROM numbers; "
        "UPDATE storage SET revision = %1;").arg(count)));

    EXPECT_EQ(write(storage_id, { makeRecord("new1", 0, "1") }).error_code(), proto::router::kErrorLimitExceeded);

    const proto::router::SyncRecordResult stored =
        write(storage_id, { makeRecord("new1", 0, "1"), makeDeletedRecord("id1", 1) });
    ASSERT_EQ(stored.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(stored), QList<qint64>({ count + 1, count + 2 }));

    EXPECT_EQ(write(storage_id, { makeRecord("new2", 0, "2") }).error_code(), proto::router::kErrorLimitExceeded);
    EXPECT_EQ(storageInfo().revision(), count + 2);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, OldestRemovedRecordsAreDropped)
{
    const std::string storage_id = create();
    const int count = proto::router::kMaxSyncDeletedRecordCount;

    ASSERT_TRUE(execRaw(QString(
        "WITH RECURSIVE numbers(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM numbers WHERE n < %1) "
        "INSERT INTO records (type, id, revision, time, deleted, data) "
        "SELECT 1, 'id' || n, n, 0, 1, X'' FROM numbers; "
        "UPDATE storage SET revision = %1;").arg(count)));

    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordList kept = records(storage_id, 0, 1);
    ASSERT_EQ(kept.record_size(), 1);
    EXPECT_EQ(kept.record(0).id(), "id1");

    ASSERT_EQ(write(storage_id, { makeDeletedRecord("a", count + 1) }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordList oldest = records(storage_id, 0, 1);
    ASSERT_EQ(oldest.record_size(), 1);
    EXPECT_EQ(oldest.record(0).id(), "id2");

    const proto::router::SyncRecordList newest = records(storage_id, count + 1);
    ASSERT_EQ(newest.record_size(), 1);
    EXPECT_EQ(newest.record(0).id(), "a");
    EXPECT_TRUE(newest.record(0).deleted());
}

//--------------------------------------------------------------------------------------------------
// A device that missed a dropped removal bases its change on a revision the storage no longer has.
TEST_F(SyncStorageTest, ChangeOfADroppedRecordIsStoredAsNew)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    const proto::router::SyncRecordResult result =
        write(storage_id, { makeRecord("dropped", 5, "2"), makeDeletedRecord("removed", 7) });
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(result), QList<qint64>({ 2, 3 }));

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 3);
    EXPECT_EQ(recordIds(list), QList<std::string>({ "a", "dropped", "removed" }));
    EXPECT_EQ(list.record(1).data(), "2");
    EXPECT_TRUE(list.record(2).deleted());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, StorageSurvivesReopening)
{
    const std::string storage_id = create();
    ASSERT_EQ(write(storage_id, { makeRecord("a", 0, "1") }).error_code(), proto::router::kErrorOk);

    storage_.close();
    EXPECT_FALSE(storage_.isValid());
    EXPECT_EQ(storageInfo().error_code(), proto::router::kErrorInternalError);

    ASSERT_TRUE(storage_.open(kFirstUserGuid));

    const proto::router::SyncStorageInfo read = storageInfo();
    ASSERT_EQ(read.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(read.storage_id(), storage_id);
    EXPECT_EQ(read.revision(), 1);

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).data(), "1");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, StoragesOfTheUsersAreFilesOfOneDirectory)
{
    EXPECT_TRUE(QFile::exists(file_path_));
    EXPECT_EQ(userGuids(), QStringList({ kFirstUserGuid }));

    SyncStorage second;
    ASSERT_TRUE(second.open(kSecondUserGuid));
    EXPECT_TRUE(QFile::exists(directory_path_ + '/' + kSecondUserGuid + ".db3"));

    const std::string storage_id = create();
    ASSERT_FALSE(storage_id.empty());

    proto::router::SyncStorageInfo second_info;
    second.readStorageInfo(&second_info);
    EXPECT_EQ(second_info.error_code(), proto::router::kErrorNotFound);

    EXPECT_EQ(userGuids(), QStringList({ kFirstUserGuid, kSecondUserGuid }));

    SyncStorage invalid;
    EXPECT_FALSE(invalid.open(QString()));
    EXPECT_FALSE(invalid.open("../router"));
    EXPECT_FALSE(invalid.open(QString("{") + kSecondUserGuid + '}'));
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, UserGuidsSkipTheFilesOfOtherNames)
{
    const QString guid = kSecondUserGuid;

    const QString names[] = { QString("notes.db3"), QString("1.db3"), QString('{' + guid + "}.db3"),
                              QString(guid.toUpper().replace('2', 'A') + ".db3"), QString(guid + ".db3.bak"),
                              QString(guid + ".txt") };

    for (const QString& name : names)
    {
        ASSERT_TRUE(createFile(directory_path_ + '/' + name));
    }

    EXPECT_EQ(userGuids(), QStringList({ kFirstUserGuid }));
}

//--------------------------------------------------------------------------------------------------
// Two connections to the storage of one user see the changes of each other.
TEST_F(SyncStorageTest, ConnectionsToOneStorageShareItsRecords)
{
    const std::string storage_id = create();
    ASSERT_FALSE(storage_id.empty());

    SyncStorage second;
    ASSERT_TRUE(second.open(kFirstUserGuid));

    proto::router::SyncRecordRequest request;
    request.set_storage_id(storage_id);
    *request.add_record() = makeRecord("a", 0, "1");

    proto::router::SyncRecordResult stored;
    bool records_changed = false;
    second.storeRecords(request, &stored, &records_changed);
    ASSERT_EQ(stored.error_code(), proto::router::kErrorOk);
    EXPECT_TRUE(records_changed);

    const proto::router::SyncRecordList list = records(storage_id);
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(recordIds(list), QList<std::string>({ "a" }));

    const proto::router::SyncRecordResult next = write(storage_id, { makeRecord("b", 0, "2") });
    ASSERT_EQ(next.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(next), QList<qint64>({ 2 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RecordsChangeOnlyWhenARecordIsStored)
{
    const std::string storage_id = create();
    ASSERT_FALSE(storage_id.empty());

    proto::router::SyncRecordRequest request;
    request.set_storage_id(storage_id);
    *request.add_record() = makeRecord("a", 0, "1");

    proto::router::SyncRecordResult stored;
    bool records_changed = false;
    storage_.storeRecords(request, &stored, &records_changed);
    ASSERT_EQ(stored.error_code(), proto::router::kErrorOk);
    EXPECT_TRUE(records_changed);

    proto::router::SyncRecordResult refused;
    storage_.storeRecords(request, &refused, &records_changed);
    ASSERT_EQ(refused.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(revisions(refused), QList<qint64>({ 0 }));
    EXPECT_FALSE(records_changed);

    request.set_storage_id("another storage");
    records_changed = true;

    proto::router::SyncRecordResult missed;
    storage_.storeRecords(request, &missed, &records_changed);
    EXPECT_EQ(missed.error_code(), proto::router::kErrorNotFound);
    EXPECT_FALSE(records_changed);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncStorageTest, RemoveDeletesTheFilesOfTheUser)
{
    ASSERT_FALSE(create().empty());
    storage_.close();

    ASSERT_TRUE(createFile(file_path_ + "-wal"));
    ASSERT_TRUE(createFile(file_path_ + "-shm"));
    ASSERT_TRUE(createFile(directory_path_ + '/' + kSecondUserGuid + ".db3"));

    EXPECT_TRUE(SyncStorage::remove(kFirstUserGuid));
    EXPECT_FALSE(QFile::exists(file_path_));
    EXPECT_FALSE(QFile::exists(file_path_ + "-wal"));
    EXPECT_FALSE(QFile::exists(file_path_ + "-shm"));
    EXPECT_EQ(userGuids(), QStringList({ kSecondUserGuid }));

    EXPECT_TRUE(SyncStorage::remove(kFirstUserGuid));
    EXPECT_FALSE(SyncStorage::remove(QString()));
    EXPECT_FALSE(SyncStorage::remove("../router"));
}
