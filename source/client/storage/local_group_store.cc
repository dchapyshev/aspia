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

#include "client/storage/local_group_store.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/sync_store.h"

namespace {

//--------------------------------------------------------------------------------------------------
LocalGroupConfig readLocalGroup(const SqlQuery& query)
{
    LocalGroupConfig group;
    group.setId(query.columnInt64(0));
    group.setParentId(query.columnInt64(1));
    group.setName(query.columnText(2));
    group.setComment(query.columnText(3));
    group.setGuid(query.columnText(4));
    group.setModifyTime(query.columnInt64(5));
    return group;
}

//--------------------------------------------------------------------------------------------------
// True when the new parent is the group itself or one of the groups below it. Such a move closes a
// loop: the group and everything under it drop out of the tree, which is walked from the root down,
// and a path built by walking parents upward from a host inside the loop never ends.
bool wouldCloseLoop(SqlDatabase& db, qint64 group_id, qint64 new_parent_id)
{
    // The root is where the tree starts and is below nothing.
    if (new_parent_id <= 0)
        return false;

    if (new_parent_id == group_id)
        return true;

    // Walks the parents of the new parent upward. On a tree that is still whole the walk ends at
    // the root, and SQLite caps a runaway recursion by itself.
    const char kSql[] =
        "WITH RECURSIVE ancestors(id) AS ("
        "    SELECT id FROM local_groups WHERE id=?"
        "    UNION ALL"
        "    SELECT g.parent_id FROM local_groups g JOIN ancestors a ON g.id = a.id "
        "      WHERE g.parent_id IS NOT NULL"
        ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1";

    SqlQuery query(db, kSql);
    query.addInt64(new_parent_id);
    query.addInt64(group_id);

    return query.next() == SqlQuery::StepResult::ROW;
}

//--------------------------------------------------------------------------------------------------
bool readGroupTree(SqlDatabase& db, qint64 group_id, std::vector<qint64>* group_ids)
{
    group_ids->assign(1, group_id);

    for (size_t i = 0; i < group_ids->size(); ++i)
    {
        SqlQuery children_query(db, "SELECT id FROM local_groups WHERE parent_id=?");
        children_query.addInt64((*group_ids)[i]);

        for (;;)
        {
            const SqlQuery::StepResult step = children_query.next();
            if (step == SqlQuery::StepResult::FAILED)
            {
                LOG(ERROR) << "Unable to execute query:" << db.lastError();
                return false;
            }
            if (step == SqlQuery::StepResult::DONE)
                break;

            group_ids->emplace_back(children_query.columnInt64(0));
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool removeGroupTree(SqlDatabase& db, qint64 group_id)
{
    std::vector<qint64> removed_group_ids;
    if (!readGroupTree(db, group_id, &removed_group_ids))
        return false;

    const qint64 modify_time = QDateTime::currentSecsSinceEpoch();

    for (qint64 removed_group_id : removed_group_ids)
    {
        SqlQuery remove_hosts_query(db, "DELETE FROM local_hosts WHERE group_id=? AND sync_revision=0");
        remove_hosts_query.addInt64(removed_group_id);

        if (!remove_hosts_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }

        SqlQuery mark_hosts_query(db, "UPDATE local_hosts SET group_id=NULL, name='', comment='', data=X'', "
                                       "credential_id=NULL, os_credential_id=NULL, profile_id=NULL, "
                                       "modify_time=?, sync_dirty=sync_dirty+1, sync_state=? "
                                       "WHERE group_id=? AND sync_revision!=0");
        mark_hosts_query.addInt64(modify_time);
        mark_hosts_query.addInt64(SyncStore::SYNC_STATE_DELETED);
        mark_hosts_query.addInt64(removed_group_id);

        if (!mark_hosts_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }

        SqlQuery mark_group_query(db, "UPDATE local_groups SET parent_id=NULL, name='', comment='', modify_time=?, "
                                       "sync_dirty=sync_dirty+1, data=X'', sync_state=? "
                                       "WHERE id=? AND sync_revision!=0 AND sync_state!=?");
        mark_group_query.addInt64(modify_time);
        mark_group_query.addInt64(SyncStore::SYNC_STATE_DELETED);
        mark_group_query.addInt64(removed_group_id);
        mark_group_query.addInt64(SyncStore::SYNC_STATE_DELETED);

        if (!mark_group_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }

    // Erasing a group erases the rows of the groups below it, so it is done once every group the
    // router holds is marked and has left its parent.
    for (qint64 removed_group_id : removed_group_ids)
    {
        SqlQuery remove_group_query(db, "DELETE FROM local_groups WHERE id=? AND sync_revision=0");
        remove_group_query.addInt64(removed_group_id);

        if (!remove_group_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool setStateBelow(SqlDatabase& db, qint64 group_id, SyncStore::SyncState state)
{
    const SyncStore::SyncState previous_state =
        state == SyncStore::SYNC_STATE_WAITING ? SyncStore::SYNC_STATE_PRESENT : SyncStore::SYNC_STATE_WAITING;

    std::vector<qint64> group_ids;
    if (!readGroupTree(db, group_id, &group_ids))
        return false;

    for (qint64 current_group_id : group_ids)
    {
        SqlQuery hosts_query(db, "UPDATE local_hosts SET sync_state=? WHERE group_id=? AND sync_state=?");
        hosts_query.addInt64(state);
        hosts_query.addInt64(current_group_id);
        hosts_query.addInt64(previous_state);

        if (!hosts_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }

        SqlQuery groups_query(db, "UPDATE local_groups SET sync_state=? WHERE parent_id=? AND sync_state=?");
        groups_query.addInt64(state);
        groups_query.addInt64(current_group_id);
        groups_query.addInt64(previous_state);

        if (!groups_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
LocalGroupStore::LocalGroupStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::list(qint64 parent_id, QList<LocalGroupConfig>* groups) const
{
    CHECK(groups);
    groups->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid, modify_time FROM local_groups "
                          "WHERE parent_id IS NULLIF(?, 0) AND sync_state=0 ORDER BY casefold(name)");
    query.addInt64(parent_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        groups->append(readLocalGroup(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::all(QList<LocalGroupConfig>* groups) const
{
    CHECK(groups);
    groups->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid, modify_time FROM local_groups "
                          "WHERE sync_state=0 ORDER BY casefold(name)");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        groups->append(readLocalGroup(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::add(LocalGroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!group.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (group.guid().isEmpty())
        group.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    SqlQuery query(db(), "INSERT INTO local_groups (id, parent_id, name, comment, guid, modify_time, sync_dirty) "
                          "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, 1)");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addText(group.guid());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    group.setId(db().lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::modify(const LocalGroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!group.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (wouldCloseLoop(db(), group.id(), group.parentId()))
    {
        LOG(ERROR) << "Group" << group.id() << "cannot be put under" << group.parentId();
        return false;
    }

    SqlQuery query(db(), "UPDATE local_groups SET parent_id=NULLIF(?, 0), name=?, comment=?, modify_time=?, "
                          "sync_dirty=sync_dirty+1 WHERE id=? AND sync_state=0");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(group.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (db().changes() == 0)
    {
        LOG(ERROR) << "Local group" << group.id() << "not found";
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::move(qint64 group_id, qint64 new_parent_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (wouldCloseLoop(db(), group_id, new_parent_id))
    {
        LOG(ERROR) << "Group" << group_id << "cannot be put under" << new_parent_id;
        return false;
    }

    SqlQuery query(db(), "UPDATE local_groups SET parent_id=NULLIF(?, 0), modify_time=?, sync_dirty=sync_dirty+1 "
                          "WHERE id=? AND sync_state=0");
    query.addInt64(new_parent_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (db().changes() == 0)
    {
        LOG(ERROR) << "Local group" << group_id << "not found";
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::remove(qint64 group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The root is the parent every group of the top level names and not a record of its own.
    if (group_id <= 0)
    {
        LOG(ERROR) << "Invalid group id:" << group_id;
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (!removeGroupTree(db(), group_id))
        return false;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult LocalGroupStore::find(qint64 group_id, LocalGroupConfig* group) const
{
    CHECK(group);
    *group = LocalGroupConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid, modify_time FROM local_groups "
                          "WHERE id=? AND sync_state=0");
    query.addInt64(group_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    *group = readLocalGroup(query);
    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult LocalGroupStore::findRowIdByGuid(const QString& guid, qint64* group_id) const
{
    CHECK(group_id);
    *group_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id FROM local_groups WHERE guid=?");
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    *group_id = query.columnInt64(0);
    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult LocalGroupStore::findGuidByRowId(qint64 group_id, QString* guid) const
{
    CHECK(guid);
    guid->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT guid FROM local_groups WHERE id=?");
    query.addInt64(group_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    *guid = query.columnText(0);
    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult LocalGroupStore::unsynced(QList<Unsynced>* groups) const
{
    CHECK(groups);
    groups->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid, modify_time, sync_revision, "
                          "sync_dirty, data, sync_state FROM local_groups WHERE sync_dirty!=0");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        Unsynced& unsynced = groups->emplaceBack();
        unsynced.group = readLocalGroup(query);
        unsynced.deleted = query.columnInt64(9) == SyncStore::SYNC_STATE_DELETED;
        unsynced.sync_revision = query.columnInt64(6);
        unsynced.sync_dirty = query.columnInt64(7);

        if (!unsynced.deleted && !unsynced.group.setSerializedData(query.columnBlob(8)))
        {
            LOG(ERROR) << "Unable to read data of group" << unsynced.group.id();
            unsynced.damaged = true;
            result = Database::ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::storeReceived(
    SyncTransaction& transaction, const LocalGroupConfig& group, qint64 sync_revision)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (!group.isValid() || group.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    qint64 group_id = 0;
    qint64 stored_revision = 0;
    qint64 stored_state = SyncStore::SYNC_STATE_PRESENT;

    {
        SqlQuery row(db(), "SELECT id, sync_revision, sync_state FROM local_groups WHERE guid=?");
        row.addText(group.guid());

        const SqlQuery::StepResult step = row.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            group_id = row.columnInt64(0);
            stored_revision = row.columnInt64(1);
            stored_state = row.columnInt64(2);
        }
    }

    if (stored_state == SyncStore::SYNC_STATE_DELETED)
    {
        SqlQuery query(db(), "UPDATE local_groups SET sync_revision=? WHERE id=?");
        query.addInt64(sync_revision);
        query.addInt64(group_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        LOG(INFO) << "Group" << group_id << "is removed on the device and stays removed, revision:"
                  << sync_revision;
        return true;
    }

    if (group_id > 0 && stored_revision == sync_revision)
        return true;

    qint64 parent_id = group.parentId();
    qint64 sync_dirty = 0;

    if (group_id > 0 && wouldCloseLoop(db(), group_id, parent_id))
    {
        LOG(INFO) << "Received group" << group_id << "would close a loop under group" << parent_id
                  << "and is put at the root";
        parent_id = 0;
        sync_dirty = 1;
    }

    qint64 parent_state = SyncStore::SYNC_STATE_PRESENT;

    if (parent_id > 0)
    {
        SqlQuery parent_query(db(), "SELECT sync_state FROM local_groups WHERE id=?");
        parent_query.addInt64(parent_id);

        if (parent_query.next() != SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "Unable to read group" << parent_id << "of the received group:" << db().lastError();
            return false;
        }

        parent_state = parent_query.columnInt64(0);
    }

    // A group whose parent has not come, or waits itself, is not in the tree yet.
    const bool waits = parent_state == SyncStore::SYNC_STATE_EMPTY || parent_state == SyncStore::SYNC_STATE_WAITING;
    const SyncStore::SyncState state = waits ? SyncStore::SYNC_STATE_WAITING : SyncStore::SYNC_STATE_PRESENT;
    const bool existed = group_id > 0;

    if (!existed)
    {
        SqlQuery query(db(), "INSERT INTO local_groups (id, parent_id, name, comment, guid, modify_time, "
                              "sync_revision, data, sync_state) VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?)");
        query.addInt64(parent_id);
        query.addText(group.name());
        query.addText(group.comment());
        query.addText(group.guid());
        query.addInt64(group.modifyTime());
        query.addInt64(sync_revision);
        query.addBlob(group.serializedData());
        query.addInt64(state);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        group_id = db().lastInsertRowId();
    }
    else
    {
        SqlQuery query(db(), "UPDATE local_groups SET parent_id=NULLIF(?, 0), name=?, comment=?, modify_time=?, "
                              "sync_revision=?, sync_dirty=?, data=?, sync_state=? WHERE id=?");
        query.addInt64(parent_id);
        query.addText(group.name());
        query.addText(group.comment());
        query.addInt64(group.modifyTime());
        query.addInt64(sync_revision);
        query.addInt64(sync_dirty);
        query.addBlob(group.serializedData());
        query.addInt64(state);
        query.addInt64(group_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }

    if (parent_state == SyncStore::SYNC_STATE_DELETED)
    {
        LOG(INFO) << "Received group" << group_id << "lies in a group removed on the device and is removed too";
        return removeGroupTree(db(), group_id);
    }

    if (waits)
        LOG(INFO) << "Received group" << group_id << "waits for group" << parent_id;

    if (!existed || stored_state == state)
        return true;

    return setStateBelow(db(), group_id, state);
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::eraseReceived(SyncTransaction& transaction, const QString& guid)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    qint64 group_id = 0;
    const Database::FindResult found = findRowIdByGuid(guid, &group_id);
    if (found == Database::FindResult::FAILED)
        return false;

    if (found == Database::FindResult::NOT_FOUND)
        return true;

    LOG(INFO) << "Group" << group_id << "is removed by the router with what lies below it";

    if (!removeGroupTree(db(), group_id))
        return false;

    SqlQuery query(db(), "DELETE FROM local_groups WHERE id=?");
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::addEmptyRow(const QString& guid, qint64* group_id)
{
    CHECK(group_id);
    *group_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "INSERT INTO local_groups (id, guid, sync_state) VALUES (NULL, ?, ?)");
    query.addText(guid);
    query.addInt64(SyncStore::SYNC_STATE_EMPTY);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    *group_id = db().lastInsertRowId();
    return true;
}
