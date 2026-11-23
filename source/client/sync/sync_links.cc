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

#include "client/sync/sync_links.h"

#include "base/logging.h"
#include "client/storage/credential_store.h"
#include "client/storage/local_group_store.h"
#include "client/storage/profile_store.h"

//--------------------------------------------------------------------------------------------------
// static
std::optional<QString> SyncLinks::credentialGuid(Database& db, qint64 credential_id)
{
    if (credential_id <= 0)
        return QString();

    QString guid;
    const Database::FindResult found = CredentialStore(db).findGuidByRowId(credential_id, &guid);
    if (found == Database::FindResult::NOT_FOUND)
        return QString();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read credentials" << credential_id;
        return std::nullopt;
    }

    return guid;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QString> SyncLinks::profileGuid(Database& db, qint64 profile_id)
{
    if (profile_id <= 0)
        return QString();

    QString guid;
    const Database::FindResult found = ProfileStore(db).findGuidByRowId(profile_id, &guid);
    if (found == Database::FindResult::NOT_FOUND)
        return QString();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read profile" << profile_id;
        return std::nullopt;
    }

    return guid;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QString> SyncLinks::groupGuid(Database& db, qint64 group_id)
{
    if (group_id <= 0)
        return QString();

    QString guid;
    const Database::FindResult found = LocalGroupStore(db).findGuidByRowId(group_id, &guid);
    if (found == Database::FindResult::NOT_FOUND)
        return QString();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read group" << group_id;
        return std::nullopt;
    }

    return guid;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<qint64> SyncLinks::credentialId(Database& db, const std::string& guid)
{
    if (guid.empty())
        return 0;

    qint64 credential_id = 0;
    const Database::FindResult found =
        CredentialStore(db).findRowIdByGuid(QString::fromStdString(guid), &credential_id);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(INFO) << "Credentials with guid" << guid << "are not on the device, an empty row keeps the link";

        if (!CredentialStore(db).addEmptyRow(QString::fromStdString(guid), &credential_id))
            return std::nullopt;

        return credential_id;
    }

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read credentials with guid" << guid;
        return std::nullopt;
    }

    return credential_id;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<qint64> SyncLinks::profileId(Database& db, const std::string& guid)
{
    if (guid.empty())
        return 0;

    qint64 profile_id = 0;
    const Database::FindResult found = ProfileStore(db).findRowIdByGuid(QString::fromStdString(guid), &profile_id);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(INFO) << "Profile with guid" << guid << "is not on the device, an empty row keeps the link";

        if (!ProfileStore(db).addEmptyRow(QString::fromStdString(guid), &profile_id))
            return std::nullopt;

        return profile_id;
    }

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read profile with guid" << guid;
        return std::nullopt;
    }

    return profile_id;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<qint64> SyncLinks::groupId(Database& db, const std::string& guid)
{
    if (guid.empty())
        return 0;

    qint64 group_id = 0;
    const Database::FindResult found = LocalGroupStore(db).findRowIdByGuid(QString::fromStdString(guid), &group_id);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(INFO) << "Group with guid" << guid << "is not on the device, an empty row keeps its place";

        if (!LocalGroupStore(db).addEmptyRow(QString::fromStdString(guid), &group_id))
            return std::nullopt;

        return group_id;
    }

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read group with guid" << guid;
        return std::nullopt;
    }

    return group_id;
}
