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

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(sql(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                          "WHERE parent_id IS NULLIF(?, 0) ORDER BY casefold(name)");
    query.addInt64(parent_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(sql(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                          "ORDER BY casefold(name)");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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
    if (!db_.isValid())
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

    SqlQuery query(sql(), "INSERT INTO local_groups (id, parent_id, name, comment, guid, modify_time) "
                          "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?)");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addText(group.guid());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    group.setId(sql().lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::modify(const LocalGroupConfig& group)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!group.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (wouldCloseLoop(sql(), group.id(), group.parentId()))
    {
        LOG(ERROR) << "Group" << group.id() << "cannot be put under" << group.parentId();
        return false;
    }

    SqlQuery query(sql(), "UPDATE local_groups SET parent_id=NULLIF(?, 0), name=?, comment=?, modify_time=? "
                          "WHERE id=?");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(group.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::move(qint64 group_id, qint64 new_parent_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (wouldCloseLoop(sql(), group_id, new_parent_id))
    {
        LOG(ERROR) << "Group" << group_id << "cannot be put under" << new_parent_id;
        return false;
    }

    SqlQuery query(sql(), "UPDATE local_groups SET parent_id=NULLIF(?, 0), modify_time=? WHERE id=?");
    query.addInt64(new_parent_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    if (sql().changes() == 0)
    {
        LOG(ERROR) << "Local group" << group_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupStore::remove(qint64 group_id)
{
    if (!db_.isValid())
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

    SqlQuery query(sql(), "DELETE FROM local_groups WHERE id=?");
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult LocalGroupStore::find(qint64 group_id, LocalGroupConfig* group) const
{
    CHECK(group);
    *group = LocalGroupConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                          "WHERE id=?");
    query.addInt64(group_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    *group = readLocalGroup(query);
    return Database::FindResult::FOUND;
}
