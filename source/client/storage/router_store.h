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

#ifndef CLIENT_STORAGE_ROUTER_STORE_H
#define CLIENT_STORAGE_ROUTER_STORE_H

#include "client/storage/database_store.h"

class RouterStore final : public DatabaseStore
{
public:
    explicit RouterStore(Database& db = Database::instance());

    Database::FindResult find(RouterConfig* router) const;
    bool save(const RouterConfig& router);
    Database::SyncResult setSyncStorage(const RouterConfig& router, const QByteArray& storage_id,
        const SecureByteArray& key, bool reset);
    bool setSyncStorageResetPending(const RouterConfig& router);

private:
    friend class Database;
    friend class DatabaseMigration;

    bool writeConfig(const RouterConfig& router);
    bool clearConfig();
};

#endif // CLIENT_STORAGE_ROUTER_STORE_H
