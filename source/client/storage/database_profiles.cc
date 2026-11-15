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

//--------------------------------------------------------------------------------------------------
Database::ReadResult readProfile(const SqlQuery& query, ProfileConfig* profile)
{
    profile->setId(query.columnInt64(0));
    profile->setName(query.columnText(1));
    profile->setGuid(query.columnText(3));

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

    SqlQuery query(db_, "SELECT id, name, data, guid FROM profiles ORDER BY guid<>?, casefold(name)");
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

    SqlQuery query(db_, "INSERT INTO profiles (id, name, data, guid, modify_time) VALUES (NULL, ?, ?, ?, ?)");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addText(profile.guid());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    profile.setId(db_.lastInsertRowId());
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

    SqlQuery query(db_, "UPDATE profiles SET name=?, data=?, modify_time=? WHERE id=?");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(profile.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Profile" << profile.id() << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeProfile(qint64 profile_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM profiles WHERE id=? AND guid<>?");
    query.addInt64(profile_id);
    query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Profile" << profile_id << "not found or is the default one";
        return false;
    }

    return true;
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

    SqlQuery query(db_, "SELECT id, name, data, guid FROM profiles WHERE id=?");
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
        SqlQuery query(db_, "SELECT id, name, data, guid FROM profiles WHERE guid=?");
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

    SqlQuery query(db_, "UPDATE profiles SET modify_time=0 WHERE id=?");
    query.addInt64(profile.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    LOG(INFO) << "Default profile created";
    return true;
}
