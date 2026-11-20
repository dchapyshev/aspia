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

#ifndef CLIENT_STORAGE_ROUTER_HOST_STORE_H
#define CLIENT_STORAGE_ROUTER_HOST_STORE_H

#include "client/storage/database_store.h"

class RouterHostStore final : public DatabaseStore
{
public:
    explicit RouterHostStore(Database& db = Database::instance());

    Database::ReadResult list(QList<RouterHostConfig>* hosts) const;
    bool add(const RouterHostConfig& host);
    bool modify(const RouterHostConfig& host);
    bool remove(HostId host_id);
    Database::FindResult find(HostId host_id, RouterHostConfig* host) const;
    std::optional<std::pair<QString, SecureString>> credentials(HostId host_id) const;
    bool outdated(QList<HostId>* hosts) const;
    bool updateCheckTime(HostId host_id);
    bool setOsCredentialId(HostId host_id, qint64 credential_id);
    bool setProfileId(HostId host_id, qint64 profile_id);
};

#endif // CLIENT_STORAGE_ROUTER_HOST_STORE_H
