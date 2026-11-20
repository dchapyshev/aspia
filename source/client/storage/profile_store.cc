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

#include "client/storage/profile_store.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_query.h"

namespace {

//--------------------------------------------------------------------------------------------------
Database::ReadResult readProfile(const SqlQuery& query, ProfileConfig* profile)
{
    profile->setId(query.columnInt64(0));
    profile->setName(query.columnText(1));
    profile->setGuid(query.columnText(3));
    profile->setModifyTime(query.columnInt64(4));

    if (!profile->setSerializedData(query.columnBlob(2)))
    {
        LOG(ERROR) << "Unable to read data of profile" << profile->id();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ProfileStore::ProfileStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult ProfileStore::list(QList<ProfileConfig>* profiles) const
{
    CHECK(profiles);
    profiles->clear();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, name, data, guid, modify_time FROM profiles WHERE deleted=0 "
                          "ORDER BY guid<>?, casefold(name)");
    query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        ProfileConfig profile;
        if (readProfile(query, &profile) != Database::ReadResult::OK)
            result = Database::ReadResult::INCOMPLETE;

        profiles->append(profile);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::add(ProfileConfig& profile)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!profile.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (profile.guid().isEmpty())
        profile.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    const qint64 modify_time = QDateTime::currentSecsSinceEpoch();

    SqlQuery query(sql(), "INSERT INTO profiles (id, name, data, guid, modify_time, sync_dirty) "
                          "VALUES (NULL, ?, ?, ?, ?, 1)");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addText(profile.guid());
    query.addInt64(modify_time);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    profile.setId(sql().lastInsertRowId());
    profile.setModifyTime(modify_time);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::modify(const ProfileConfig& profile)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!profile.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery row(sql(), "SELECT data FROM profiles WHERE id=? AND deleted=0");
    row.addInt64(profile.id());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        else
            LOG(ERROR) << "Profile" << profile.id() << "not found";
        return false;
    }

    ProfileConfig stored;
    if (!stored.setSerializedData(row.columnBlob(0)))
        LOG(WARNING) << "Stored data of profile" << profile.id() << "could not be read and is replaced";

    ProfileConfig modified = profile;
    modified.setUnknownData(stored.unknownData());

    SqlQuery query(sql(), "UPDATE profiles SET name=?, data=?, modify_time=?, sync_dirty=sync_dirty+1 WHERE id=?");
    query.addText(modified.name());
    query.addBlob(modified.serializedData());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(modified.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::remove(qint64 profile_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery remove_query(sql(), "DELETE FROM profiles WHERE id=? AND guid<>? AND sync_revision=0");
    remove_query.addInt64(profile_id);
    remove_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!remove_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    const int removed = sql().changes();

    SqlQuery mark_query(sql(), "UPDATE profiles SET name='', data=X'', modify_time=?, sync_dirty=sync_dirty+1, "
                               "deleted=1 WHERE id=? AND guid<>? AND sync_revision!=0 AND deleted=0");
    mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
    mark_query.addInt64(profile_id);
    mark_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!mark_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    if (removed + sql().changes() == 0)
    {
        LOG(ERROR) << "Profile" << profile_id << "not found or is the default one";
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::FindResult ProfileStore::find(qint64 profile_id, ProfileConfig* profile) const
{
    CHECK(profile);
    *profile = ProfileConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, name, data, guid, modify_time FROM profiles WHERE id=? AND deleted=0");
    query.addInt64(profile_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readProfile(query, profile) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of profile" << profile_id;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult ProfileStore::findByGuid(const QString& guid, ProfileConfig* profile) const
{
    CHECK(profile);
    *profile = ProfileConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    if (guid.isEmpty())
        return Database::FindResult::NOT_FOUND;

    SqlQuery query(sql(), "SELECT id, name, data, guid, modify_time FROM profiles WHERE guid=? AND deleted=0");
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readProfile(query, profile) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of profile with guid" << guid;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
ProfileConfig ProfileStore::defaultProfile() const
{
    ProfileConfig profile;
    profile.setId(0);
    profile.setGuid(ProfileConfig::kDefaultGuid);

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
    }
    else
    {
        SqlQuery query(sql(), "SELECT id, name, data, guid, modify_time FROM profiles WHERE guid=?");
        query.addText(std::string_view(ProfileConfig::kDefaultGuid));

        if (query.next() == SqlQuery::StepResult::ROW)
        {
            if (readProfile(query, &profile) == Database::ReadResult::OK)
                return profile;
        }
        else
        {
            LOG(ERROR) << "Default profile is missing:" << sql().lastError();
        }
    }

    profile.setName(QString());
    profile.setDesktopConfig(defaultDesktopConfig());
    profile.setUdpMethods(UDP_METHOD_ALL);
    return profile;
}

//--------------------------------------------------------------------------------------------------
ProfileConfig ProfileStore::hostProfile(const HostConfig& host) const
{
    qint64 profile_id = 0;

    if (host.entryId() > 0)
    {
        LocalHostConfig local_host;
        const Database::FindResult found = db_.findLocalHost(host.entryId(), &local_host);
        if (found == Database::FindResult::FOUND || found == Database::FindResult::UNREADABLE)
            profile_id = local_host.profileId();
    }
    else if (isHostId(host.address()))
    {
        RouterHostConfig router_host;
        const Database::FindResult found =
            db_.findRouterHost(stringToHostId(host.address()), &router_host);
        if (found == Database::FindResult::FOUND || found == Database::FindResult::UNREADABLE)
            profile_id = router_host.profileId();
    }

    ProfileConfig profile;
    if (profile_id > 0)
    {
        if (find(profile_id, &profile) == Database::FindResult::FOUND)
            return profile;

        LOG(ERROR) << "Profile" << profile_id << "of host" << host.address() << "is not available";
    }

    return defaultProfile();
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult ProfileStore::unsynced(QList<Unsynced>* profiles) const
{
    CHECK(profiles);
    profiles->clear();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, name, data, guid, modify_time, sync_revision, sync_dirty, deleted "
                          "FROM profiles WHERE sync_dirty!=0");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        Unsynced& unsynced = profiles->emplaceBack();
        unsynced.deleted = query.columnInt64(7) != 0;
        unsynced.sync_revision = query.columnInt64(5);
        unsynced.sync_dirty = query.columnInt64(6);

        if (unsynced.deleted)
        {
            unsynced.profile.setId(query.columnInt64(0));
            unsynced.profile.setGuid(query.columnText(3));
            unsynced.profile.setModifyTime(query.columnInt64(4));
        }
        else if (readProfile(query, &unsynced.profile) != Database::ReadResult::OK)
        {
            unsynced.damaged = true;
            result = Database::ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::ensureDefaultProfile()
{
    {
        SqlQuery query(sql(), "SELECT 1 FROM profiles WHERE guid=?");
        query.addText(std::string_view(ProfileConfig::kDefaultGuid));

        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::ROW)
            return true;

        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return false;
        }
    }

    ProfileConfig profile;
    profile.setGuid(ProfileConfig::kDefaultGuid);
    profile.setDesktopConfig(defaultDesktopConfig());
    profile.setUdpMethods(UDP_METHOD_ALL);

    if (!add(profile))
        return false;

    SqlQuery query(sql(), "UPDATE profiles SET modify_time=0, sync_dirty=0 WHERE id=?");
    query.addInt64(profile.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    LOG(INFO) << "Default profile created";
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::storeReceived(
    Database::SyncTransaction& transaction, const ProfileConfig& profile, qint64 sync_revision)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (!profile.isValid() || profile.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(sql(), "INSERT INTO profiles (id, name, data, guid, modify_time, sync_revision) "
                          "VALUES (NULL, ?, ?, ?, ?, ?) "
                          "ON CONFLICT(guid) DO UPDATE SET name=excluded.name, data=excluded.data, "
                          "modify_time=excluded.modify_time, sync_revision=excluded.sync_revision, sync_dirty=0, "
                          "deleted=0 WHERE profiles.sync_revision!=excluded.sync_revision");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addText(profile.guid());
    query.addInt64(profile.modifyTime());
    query.addInt64(sync_revision);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ProfileStore::setUnknownData(
    Database::SyncTransaction& transaction, const QString& guid, const QByteArray& unknown_data)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlQuery row(sql(), "SELECT data FROM profiles WHERE guid=? AND deleted=0");
    row.addText(guid);

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        else
            LOG(ERROR) << "Profile with guid" << guid << "not found";
        return false;
    }

    ProfileConfig profile;
    if (!profile.setSerializedData(row.columnBlob(0)))
    {
        LOG(ERROR) << "Unable to read data of profile with guid" << guid;
        return false;
    }

    profile.setUnknownData(unknown_data);

    SqlQuery query(sql(), "UPDATE profiles SET data=? WHERE guid=?");
    query.addBlob(profile.serializedData());
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}
