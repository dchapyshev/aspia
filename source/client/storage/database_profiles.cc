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

#include "client/storage/database.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/net/udp_channel.h"
#include "base/peer/host_id.h"
#include "base/sql/sql_query.h"

namespace {

const char kSettingReceivedProfilesRevision[] = "received_profiles_revision";

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
Database::ReadResult Database::profileList(QList<ProfileConfig>* profiles) const
{
    CHECK(profiles);
    profiles->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, name, data, guid, modify_time FROM profiles WHERE deleted=0 "
                        "ORDER BY guid<>?, casefold(name)");
    query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        ProfileConfig profile;
        if (readProfile(query, &profile) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        profiles->append(profile);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::addProfile(ProfileConfig& profile)
{
    if (!isValid())
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

    SqlQuery query(db_, "INSERT INTO profiles (id, name, data, guid, modify_time, sync_dirty) "
                        "VALUES (NULL, ?, ?, ?, ?, 1)");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addText(profile.guid());
    query.addInt64(modify_time);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    profile.setId(db_.lastInsertRowId());
    profile.setModifyTime(modify_time);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyProfile(const ProfileConfig& profile)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!profile.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery row(db_, "SELECT data FROM profiles WHERE id=? AND deleted=0");
    row.addInt64(profile.id());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        else
            LOG(ERROR) << "Profile" << profile.id() << "not found";
        return false;
    }

    ProfileConfig stored;
    if (!stored.setSerializedData(row.columnBlob(0)))
        LOG(WARNING) << "Stored data of profile" << profile.id() << "could not be read and is replaced";

    ProfileConfig modified = profile;
    modified.setUnknownData(stored.unknownData());

    SqlQuery query(db_, "UPDATE profiles SET name=?, data=?, modify_time=?, sync_dirty=sync_dirty+1 WHERE id=?");
    query.addText(modified.name());
    query.addBlob(modified.serializedData());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(modified.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool Database::removeProfile(qint64 profile_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery remove_query(db_, "DELETE FROM profiles WHERE id=? AND guid<>? AND sync_revision=0");
    remove_query.addInt64(profile_id);
    remove_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!remove_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    const int removed = db_.changes();

    SqlQuery mark_query(db_, "UPDATE profiles SET name='', data=X'', modify_time=?, sync_dirty=sync_dirty+1, "
                             "deleted=1 WHERE id=? AND guid<>? AND sync_revision!=0 AND deleted=0");
    mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
    mark_query.addInt64(profile_id);
    mark_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!mark_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (removed + db_.changes() == 0)
    {
        LOG(ERROR) << "Profile" << profile_id << "not found or is the default one";
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findProfile(qint64 profile_id, ProfileConfig* profile) const
{
    CHECK(profile);
    *profile = ProfileConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, name, data, guid, modify_time FROM profiles WHERE id=? AND deleted=0");
    query.addInt64(profile_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readProfile(query, profile) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of profile" << profile_id;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findProfileByGuid(const QString& guid, ProfileConfig* profile) const
{
    CHECK(profile);
    *profile = ProfileConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    if (guid.isEmpty())
        return FindResult::NOT_FOUND;

    SqlQuery query(db_, "SELECT id, name, data, guid, modify_time FROM profiles WHERE guid=? AND deleted=0");
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readProfile(query, profile) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of profile with guid" << guid;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
ProfileConfig Database::defaultProfile() const
{
    ProfileConfig profile;
    profile.setId(0);
    profile.setGuid(ProfileConfig::kDefaultGuid);

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
    }
    else
    {
        SqlQuery query(db_, "SELECT id, name, data, guid, modify_time FROM profiles WHERE guid=?");
        query.addText(std::string_view(ProfileConfig::kDefaultGuid));

        if (query.next() == SqlQuery::StepResult::ROW)
        {
            if (readProfile(query, &profile) == ReadResult::OK)
                return profile;
        }
        else
        {
            LOG(ERROR) << "Default profile is missing:" << db_.lastError();
        }
    }

    profile.setName(QString());
    profile.setDesktopConfig(defaultDesktopConfig());
    profile.setUdpMethods(UDP_METHOD_ALL);
    return profile;
}

//--------------------------------------------------------------------------------------------------
ProfileConfig Database::hostProfile(const HostConfig& host) const
{
    qint64 profile_id = 0;

    if (host.entryId() > 0)
    {
        LocalHostConfig local_host;
        const FindResult found = findLocalHost(host.entryId(), &local_host);
        if (found == FindResult::FOUND || found == FindResult::UNREADABLE)
            profile_id = local_host.profileId();
    }
    else if (isHostId(host.address()))
    {
        RouterHostConfig router_host;
        const FindResult found =
            findRouterHost(stringToHostId(host.address()), &router_host);
        if (found == FindResult::FOUND || found == FindResult::UNREADABLE)
            profile_id = router_host.profileId();
    }

    ProfileConfig profile;
    if (profile_id > 0)
    {
        if (findProfile(profile_id, &profile) == FindResult::FOUND)
            return profile;

        LOG(ERROR) << "Profile" << profile_id << "of host" << host.address() << "is not available";
    }

    return defaultProfile();
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::unsyncedProfiles(QList<UnsyncedProfile>* profiles) const
{
    CHECK(profiles);
    profiles->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, name, data, guid, modify_time, sync_revision, sync_dirty, deleted "
                        "FROM profiles WHERE sync_dirty!=0");

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        UnsyncedProfile& unsynced = profiles->emplaceBack();
        unsynced.deleted = query.columnInt64(7) != 0;
        unsynced.sync_revision = query.columnInt64(5);
        unsynced.sync_dirty = query.columnInt64(6);

        if (unsynced.deleted)
        {
            unsynced.profile.setId(query.columnInt64(0));
            unsynced.profile.setGuid(query.columnText(3));
            unsynced.profile.setModifyTime(query.columnInt64(4));
        }
        else if (readProfile(query, &unsynced.profile) != ReadResult::OK)
        {
            unsynced.damaged = true;
            result = ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::receivedProfilesRevision() const
{
    return readSetting(kSettingReceivedProfilesRevision).toLongLong();
}

//--------------------------------------------------------------------------------------------------
bool Database::ensureDefaultProfile()
{
    {
        SqlQuery query(db_, "SELECT 1 FROM profiles WHERE guid=?");
        query.addText(std::string_view(ProfileConfig::kDefaultGuid));

        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::ROW)
            return true;

        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
    }

    ProfileConfig profile;
    profile.setGuid(ProfileConfig::kDefaultGuid);
    profile.setDesktopConfig(defaultDesktopConfig());
    profile.setUdpMethods(UDP_METHOD_ALL);

    if (!addProfile(profile))
        return false;

    SqlQuery query(db_, "UPDATE profiles SET modify_time=0, sync_dirty=0 WHERE id=?");
    query.addInt64(profile.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    LOG(INFO) << "Default profile created";
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setReceivedProfilesRevision(qint64 revision)
{
    CHECK(db_.isTransactionActive());
    return writeSetting(kSettingReceivedProfilesRevision, QString::number(revision));
}
