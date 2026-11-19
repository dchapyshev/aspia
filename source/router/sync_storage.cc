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

#include "router/sync_storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_constants.h"
#include "proto/router_sync.h"

namespace {

const char kFileSuffix[] = ".db3";

QString g_test_directory_path;

//--------------------------------------------------------------------------------------------------
QString directoryPath()
{
    if (!g_test_directory_path.isEmpty())
        return g_test_directory_path;

    return BasePaths::appDataDir() + "/sync";
}

//--------------------------------------------------------------------------------------------------
QString filePath(const QString& user_guid)
{
    return directoryPath() + '/' + user_guid + kFileSuffix;
}

//--------------------------------------------------------------------------------------------------
bool isUserGuid(const QString& text)
{
    const QUuid guid(text);
    return !guid.isNull() && guid.toString(QUuid::WithoutBraces) == text;
}

//--------------------------------------------------------------------------------------------------
bool isKeyValid(const proto::router::SyncStorageKey& key)
{
    const std::string_view fields[] = { key.salt(), key.verifier(), key.data() };

    for (std::string_view field : fields)
    {
        if (field.empty() || field.size() > proto::router::kMaxSyncStorageKeyFieldSize)
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool isRecordValid(const proto::router::SyncRecord& record)
{
    if (record.revision() < 0 || record.type() <= proto::router::SYNC_DATA_TYPE_UNKNOWN)
        return false;

    if (record.id().empty() || strLength(record.id()) > proto::router::kMaxSyncRecordIdLength)
        return false;

    if (record.deleted())
        return record.data().empty();

    return !record.data().empty() && record.data().size() <= proto::router::kMaxSyncRecordDataSize;
}

//--------------------------------------------------------------------------------------------------
bool ensureSchema(SqlDatabase& db)
{
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"storage\" ("
                 "\"id\" INTEGER PRIMARY KEY CHECK (\"id\" = 1),"
                 "\"storage_id\" BLOB NOT NULL,"
                 "\"reset\" INTEGER NOT NULL DEFAULT 0,"
                 "\"revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"key_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"key_salt\" BLOB NOT NULL,"
                 "\"key_verifier\" BLOB NOT NULL,"
                 "\"key_data\" BLOB NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create the storage table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"records\" ("
                 "\"type\" INTEGER NOT NULL,"
                 "\"id\" TEXT NOT NULL,"
                 "\"revision\" INTEGER NOT NULL,"
                 "\"time\" INTEGER NOT NULL,"
                 "\"deleted\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB NOT NULL DEFAULT X'',"
                 "PRIMARY KEY(\"type\", \"id\"))"))
    {
        LOG(ERROR) << "Unable to create the records table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE INDEX IF NOT EXISTS \"records_revision\" ON \"records\"(\"type\", \"revision\")"))
    {
        LOG(ERROR) << "Unable to create the records index:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE INDEX IF NOT EXISTS \"records_deleted\" ON \"records\"(\"revision\") WHERE \"deleted\"!=0"))
    {
        LOG(ERROR) << "Unable to create the deleted records index:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// DONE stands for a user that has no storage yet.
SqlQuery::StepResult selectStorageInfo(SqlDatabase& db, proto::router::SyncStorageInfo* info)
{
    CHECK(info);

    SqlQuery query(db, "SELECT storage_id, reset, revision, key_revision, key_salt, key_verifier, key_data "
                       "FROM storage");

    const SqlQuery::StepResult step = query.next();
    if (step != SqlQuery::StepResult::ROW)
        return step;

    info->set_storage_id(query.columnBlobView(0));
    info->set_reset(query.columnInt64(1) != 0);
    info->set_revision(query.columnInt64(2));
    info->set_key_revision(query.columnInt64(3));

    proto::router::SyncStorageKey* key = info->mutable_key();
    key->set_salt(query.columnBlobView(4));
    key->set_verifier(query.columnBlobView(5));
    key->set_data(query.columnBlobView(6));

    return step;
}

//--------------------------------------------------------------------------------------------------
bool insertStorage(SqlDatabase& db, const proto::router::SyncStorageKey& key, bool reset)
{
    const QByteArray storage_id = Random::byteArray(proto::router::kSyncStorageIdSize);
    if (storage_id.size() != proto::router::kSyncStorageIdSize)
    {
        LOG(ERROR) << "Unable to generate the storage id";
        return false;
    }

    SqlQuery query(db,
        "INSERT INTO storage (id, storage_id, reset, revision, key_revision, key_salt, key_verifier, key_data) "
        "VALUES (1, ?, ?, 0, 1, ?, ?, ?)");
    query.addBlob(storage_id);
    query.addInt64(reset ? 1 : 0);
    query.addBlob(key.salt());
    query.addBlob(key.verifier());
    query.addBlob(key.data());

    return query.exec();
}

//--------------------------------------------------------------------------------------------------
bool updateKey(SqlDatabase& db, const proto::router::SyncStorageKey& key)
{
    SqlQuery query(db,
        "UPDATE storage SET key_revision=key_revision+1, key_salt=?, key_verifier=?, key_data=?");
    query.addBlob(key.salt());
    query.addBlob(key.verifier());
    query.addBlob(key.data());

    return query.exec();
}

//--------------------------------------------------------------------------------------------------
bool replaceStorage(SqlDatabase& db, const proto::router::SyncStorageKey& key)
{
    const QByteArray storage_id = Random::byteArray(proto::router::kSyncStorageIdSize);
    if (storage_id.size() != proto::router::kSyncStorageIdSize)
    {
        LOG(ERROR) << "Unable to generate the storage id";
        return false;
    }

    if (!db.exec("DELETE FROM records"))
        return false;

    LOG(INFO) << "Records dropped by the reset:" << db.changes();

    SqlQuery query(db,
        "UPDATE storage SET storage_id=?, reset=1, revision=0, key_revision=1, key_salt=?, key_verifier=?, "
        "key_data=?");
    query.addBlob(storage_id);
    query.addBlob(key.salt());
    query.addBlob(key.verifier());
    query.addBlob(key.data());

    return query.exec();
}

enum class KeyAction { CREATE, CHANGE, RESET };

//--------------------------------------------------------------------------------------------------
void storeKey(SqlDatabase& db, KeyAction action, const proto::router::SyncStorageKeyRequest& request,
    proto::router::SyncStorageInfo* out)
{
    CHECK(out);

    if (!db.isOpen())
    {
        LOG(ERROR) << "Sync storage is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isKeyValid(request.key()) || request.key_revision() < 0)
    {
        LOG(ERROR) << "Invalid sync key";
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    SqlTransaction transaction(db);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction:" << db.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    proto::router::SyncStorageInfo current;

    const SqlQuery::StepResult step = selectStorageInfo(db, &current);
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    const bool exists = step == SqlQuery::StepResult::ROW;
    bool stored = false;

    if (action == KeyAction::CREATE)
    {
        if (exists)
        {
            out->set_error_code(proto::router::kErrorAlreadyExists);
            return;
        }

        stored = insertStorage(db, request.key(), false);
    }
    else if (action == KeyAction::RESET && !exists)
    {
        stored = insertStorage(db, request.key(), true);
    }
    else
    {
        if (!exists || current.storage_id() != request.storage_id())
        {
            out->set_error_code(proto::router::kErrorNotFound);
            return;
        }

        if (action == KeyAction::RESET)
        {
            stored = replaceStorage(db, request.key());
        }
        else
        {
            if (current.key_revision() != request.key_revision())
            {
                out->set_error_code(proto::router::kErrorConflict);
                return;
            }

            stored = updateKey(db, request.key());
        }
    }

    if (!stored)
    {
        LOG(ERROR) << "Unable to store the sync key:" << db.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    proto::router::SyncStorageInfo stored_info;

    if (selectStorageInfo(db, &stored_info) != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->MergeFrom(stored_info);
    out->set_error_code(proto::router::kErrorOk);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QStringList SyncStorage::userGuids()
{
    QStringList user_guids;

    const QStringList file_names =
        QDir(directoryPath()).entryList({ QString('*') + kFileSuffix }, QDir::Files | QDir::NoSymLinks);

    for (const QString& file_name : file_names)
    {
        const QString user_guid = QFileInfo(file_name).completeBaseName();
        if (isUserGuid(user_guid))
            user_guids.append(user_guid);
    }

    return user_guids;
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncStorage::remove(const QString& user_guid)
{
    if (!isUserGuid(user_guid))
    {
        LOG(ERROR) << "Invalid user guid:" << user_guid;
        return false;
    }

    const QString file_path = filePath(user_guid);

    QFile::remove(file_path + "-wal");
    QFile::remove(file_path + "-shm");

    if (QFile::exists(file_path) && !QFile::remove(file_path))
    {
        LOG(ERROR) << "Unable to remove sync storage:" << file_path;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void SyncStorage::setDirectoryPathForTesting(const QString& directory_path)
{
    g_test_directory_path = directory_path;
}

//--------------------------------------------------------------------------------------------------
bool SyncStorage::open(const QString& user_guid)
{
    if (!isUserGuid(user_guid))
    {
        LOG(ERROR) << "Invalid user guid:" << user_guid;
        return false;
    }

    const QString file_path = filePath(user_guid);

    if (!QDir().mkpath(directoryPath()))
    {
        LOG(ERROR) << "Unable to create directory for sync storage";
        return false;
    }

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "sync storage:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA journal_mode = WAL"))
        LOG(WARNING) << "Unable to enable WAL journal mode:" << db_.lastError();

    if (!ensureSchema(db_))
    {
        db_.close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::close()
{
    db_.close();
}

//--------------------------------------------------------------------------------------------------
bool SyncStorage::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::readStorageInfo(proto::router::SyncStorageInfo* out) const
{
    CHECK(out);

    if (!isValid())
    {
        LOG(ERROR) << "Sync storage is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    const SqlQuery::StepResult step = selectStorageInfo(db_, out);
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->set_error_code(step == SqlQuery::StepResult::ROW ? proto::router::kErrorOk : proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::createStorage(
    const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out)
{
    storeKey(db_, KeyAction::CREATE, request, out);
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::changeKey(const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out)
{
    storeKey(db_, KeyAction::CHANGE, request, out);
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::resetStorage(const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out)
{
    storeKey(db_, KeyAction::RESET, request, out);
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::readRecords(
    const proto::router::SyncRecordListRequest& request, proto::router::SyncRecordList* out) const
{
    CHECK(out);

    if (!isValid())
    {
        LOG(ERROR) << "Sync storage is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (request.type() <= proto::router::SYNC_DATA_TYPE_UNKNOWN || request.revision() < 0 ||
        request.count() <= 0 || request.count() > proto::router::kMaxSyncPageSize)
    {
        LOG(ERROR) << "Invalid sync record list request";
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    proto::router::SyncStorageInfo current;

    const SqlQuery::StepResult step = selectStorageInfo(db_, &current);
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (step == SqlQuery::StepResult::DONE || current.storage_id() != request.storage_id())
    {
        out->set_error_code(proto::router::kErrorNotFound);
        return;
    }

    SqlQuery query(db_, "SELECT revision, time, id, deleted, data FROM records WHERE type=? AND revision>? "
                        "ORDER BY revision LIMIT ?");
    query.addInt64(request.type());
    query.addInt64(request.revision());
    query.addInt64(request.count() + 1);

    for (;;)
    {
        const SqlQuery::StepResult row = query.next();
        if (row == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_record();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (row == SqlQuery::StepResult::DONE)
            break;

        if (out->record_size() == request.count())
        {
            out->set_has_more(true);
            break;
        }

        proto::router::SyncRecord* record = out->add_record();
        record->set_revision(query.columnInt64(0));
        record->set_time(query.columnInt64(1));
        record->set_type(request.type());
        record->set_id(query.columnTextView(2));
        record->set_deleted(query.columnInt64(3) != 0);
        record->set_data(query.columnBlobView(4));
    }

    out->set_revision(current.revision());
    out->set_current_time(secondsSinceEpoch());
    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
void SyncStorage::storeRecords(const proto::router::SyncRecordRequest& request,
    proto::router::SyncRecordResult* out, bool* records_changed)
{
    CHECK(out);
    CHECK(records_changed);

    *records_changed = false;

    if (!isValid())
    {
        LOG(ERROR) << "Sync storage is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (request.record_size() <= 0 || request.record_size() > proto::router::kMaxSyncPageSize)
    {
        LOG(ERROR) << "Invalid sync record count:" << request.record_size();
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    for (const proto::router::SyncRecord& record : request.record())
    {
        if (!isRecordValid(record))
        {
            LOG(ERROR) << "Invalid sync record";
            out->set_error_code(proto::router::kErrorInvalidRequest);
            return;
        }
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    proto::router::SyncStorageInfo current;

    const SqlQuery::StepResult step = selectStorageInfo(db_, &current);
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (step == SqlQuery::StepResult::DONE || current.storage_id() != request.storage_id())
    {
        out->set_error_code(proto::router::kErrorNotFound);
        return;
    }

    const qint64 current_time = secondsSinceEpoch();
    qint64 revision = current.revision();

    for (const proto::router::SyncRecord& record : request.record())
    {
        qint64 stored_revision = 0;
        bool stored_deleted = false;

        {
            SqlQuery select_query(db_, "SELECT revision, deleted FROM records WHERE type=? AND id=?");
            select_query.addInt64(record.type());
            select_query.addText(record.id());

            const SqlQuery::StepResult row = select_query.next();
            if (row == SqlQuery::StepResult::FAILED)
            {
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                out->clear_record_revision();
                out->set_error_code(proto::router::kErrorInternalError);
                return;
            }

            if (row == SqlQuery::StepResult::ROW)
            {
                stored_revision = select_query.columnInt64(0);
                stored_deleted = select_query.columnInt64(1) != 0;
            }
        }

        if (stored_revision && stored_revision != record.revision() && !(stored_deleted && record.revision() == 0))
        {
            out->add_record_revision(0);
            continue;
        }

        ++revision;

        SqlQuery query(db_, "INSERT OR REPLACE INTO records (type, id, revision, time, deleted, data) "
                            "VALUES (?, ?, ?, ?, ?, ?)");
        query.addInt64(record.type());
        query.addText(record.id());
        query.addInt64(revision);
        query.addInt64(current_time);
        query.addInt64(record.deleted());
        query.addBlob(record.data());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_record_revision();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        out->add_record_revision(revision);
    }

    if (revision != current.revision())
    {
        qint64 deleted_count = 0;

        {
            SqlQuery count_query(db_, "SELECT (SELECT COUNT(*) FROM records), "
                                      "(SELECT COUNT(*) FROM records WHERE deleted!=0)");
            if (count_query.next() != SqlQuery::StepResult::ROW)
            {
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                out->clear_record_revision();
                out->set_error_code(proto::router::kErrorInternalError);
                return;
            }

            deleted_count = count_query.columnInt64(1);

            if (count_query.columnInt64(0) - deleted_count > proto::router::kMaxSyncRecordCount)
            {
                LOG(ERROR) << "Sync storage is full";
                out->clear_record_revision();
                out->set_error_code(proto::router::kErrorLimitExceeded);
                return;
            }
        }

        if (deleted_count > proto::router::kMaxSyncDeletedRecordCount)
        {
            SqlQuery drop_query(db_, "DELETE FROM records WHERE deleted!=0 AND revision IN (SELECT revision FROM "
                                     "records WHERE deleted!=0 ORDER BY revision LIMIT ?)");
            drop_query.addInt64(deleted_count - proto::router::kMaxSyncDeletedRecordCount);

            if (!drop_query.exec())
            {
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                out->clear_record_revision();
                out->set_error_code(proto::router::kErrorInternalError);
                return;
            }

            LOG(INFO) << "Deleted records dropped:" << db_.changes();
        }

        SqlQuery query(db_, "UPDATE storage SET revision=?");
        query.addInt64(revision);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_record_revision();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        out->clear_record_revision();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->set_revision(revision);
    out->set_error_code(proto::router::kErrorOk);

    *records_changed = revision != current.revision();
}
