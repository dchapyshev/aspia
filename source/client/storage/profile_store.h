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

#ifndef CLIENT_STORAGE_PROFILE_STORE_H
#define CLIENT_STORAGE_PROFILE_STORE_H

#include "client/storage/database_store.h"

class SyncTransaction;

class ProfileStore final : public DatabaseStore
{
public:
    explicit ProfileStore(Database& db = Database::instance());

    struct Unsynced
    {
        ProfileConfig profile;
        bool deleted = false;
        bool damaged = false;
        qint64 sync_revision = 0;
        qint64 sync_dirty = 0;
    };

    Database::ReadResult list(QList<ProfileConfig>* profiles) const;
    bool add(ProfileConfig& profile);
    bool modify(ProfileConfig& profile);
    bool remove(qint64 profile_id);
    Database::FindResult find(qint64 profile_id, ProfileConfig* profile) const;
    Database::FindResult findByGuid(const QString& guid, ProfileConfig* profile) const;
    Database::FindResult findRowIdByGuid(const QString& guid, qint64* profile_id) const;
    Database::FindResult findGuidByRowId(qint64 profile_id, QString* guid) const;
    ProfileConfig defaultProfile() const;
    ProfileConfig hostProfile(const HostConfig& host) const;
    Database::ReadResult unsynced(QList<Unsynced>* profiles) const;
    bool ensureDefaultProfile();

    bool storeReceived(SyncTransaction& transaction, const ProfileConfig& profile, qint64 sync_revision);
    bool addEmptyRow(const QString& guid, qint64* profile_id);
};

#endif // CLIENT_STORAGE_PROFILE_STORE_H
