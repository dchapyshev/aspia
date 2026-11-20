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

#include "client/storage/router_store.h"

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "client/storage/sync_store.h"
#include "proto/router.h"

namespace {

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouter(const SqlQuery& query, RouterConfig* router)
{
    const bool opened = router->setEncryptedData(QByteArray::fromBase64(query.columnText(0).toLatin1()));

#if defined(Q_OS_ANDROID)
    router->setSessionType(proto::router::SESSION_TYPE_OPERATOR);
#endif

    if (!opened)
    {
        LOG(ERROR) << "Unable to read encrypted data of router";
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterStore::RouterStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::FindResult RouterStore::find(RouterConfig* router) const
{
    CHECK(router);
    *router = RouterConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT value FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingRouter));

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    router->setGuid(db_.guid());

    if (readRouter(query, router) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of router";
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
bool RouterStore::save(const RouterConfig& router)
{
    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    RouterConfig stored;
    if (find(&stored) == Database::FindResult::FAILED)
        return false;

    RouterConfig config = router;

    if (config.userGuid().isEmpty() && (!stored.isSyncConfirmationNeeded(config) || stored.hasSameAccount(config)))
        config.setUserGuid(stored.userGuid());

    if (config.isSyncEnabled())
    {
        config.setSyncStorageId(stored.syncStorageId());
        config.setSyncStorageKey(stored.syncStorageKey());
        config.setSyncStorageResetPending(stored.isSyncStorageResetPending());
    }
    else
    {
        config.setSyncStorageId(QByteArray());
        config.setSyncStorageKey(SecureByteArray());
        config.setSyncStorageResetPending(false);

        if (!stored.syncStorageId().isEmpty())
        {
            LOG(INFO) << "The device is detached from the storage" << stored.syncStorageId().toHex();

            if (!SyncStore(db_).reset())
            {
                LOG(ERROR) << "Unable to detach the records from the storage";
                return false;
            }
        }
    }

    if (!writeConfig(config))
        return false;

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::SyncResult RouterStore::setSyncStorage(
    const RouterConfig& router, const QByteArray& storage_id, const SecureByteArray& key, bool reset)
{
    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return Database::SyncResult::FAILED;
    }

    RouterConfig stored;
    const Database::FindResult found = find(&stored);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(WARNING) << "The router is not saved";
        return Database::SyncResult::DETACHED;
    }

    if (found != Database::FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        return Database::SyncResult::FAILED;
    }

    if (!stored.hasSameAccount(router))
    {
        LOG(WARNING) << "The router is saved for another account";
        return Database::SyncResult::DETACHED;
    }

    if (!stored.isSyncEnabled())
    {
        LOG(WARNING) << "The synchronization is turned off";
        return Database::SyncResult::DETACHED;
    }

    const bool another_storage = stored.syncStorageId() != storage_id;

    if (another_storage)
    {
        const bool discard = reset && !stored.syncStorageId().isEmpty() && !storage_id.isEmpty() &&
            !stored.isSyncStorageResetPending();

        LOG(INFO) << "Storage" << stored.syncStorageId().toHex() << "is replaced with" << storage_id.toHex()
                  << "reset:" << reset << "reset pending:" << stored.isSyncStorageResetPending()
                  << "discard:" << discard;

        SyncStore sync_store(db_);
        if (!(discard ? sync_store.discard() : sync_store.reset()))
        {
            LOG(ERROR) << "Unable to prepare the records for another storage";
            return Database::SyncResult::FAILED;
        }
    }

    const bool reset_finished = router.isSyncStorageResetPending() && stored.isSyncStorageResetPending();

    if (another_storage || stored.syncStorageKey() != key || reset_finished)
    {
        stored.setSyncStorageId(storage_id);
        stored.setSyncStorageKey(key);

        if (reset_finished)
            stored.setSyncStorageResetPending(false);

        if (!writeConfig(stored))
        {
            LOG(ERROR) << "Unable to write the router";
            return Database::SyncResult::FAILED;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return Database::SyncResult::FAILED;
    }

    return Database::SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
bool RouterStore::setSyncStorageResetPending(const RouterConfig& router)
{
    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    RouterConfig stored;
    if (find(&stored) != Database::FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        return false;
    }

    if (!stored.hasSameAccount(router))
    {
        LOG(WARNING) << "The router is saved for another account";
        return false;
    }

    if (!stored.isSyncEnabled())
    {
        LOG(WARNING) << "The synchronization is turned off";
        return false;
    }

    if (!stored.isSyncStorageResetPending())
    {
        stored.setSyncStorageResetPending(true);

        if (!writeConfig(stored))
        {
            LOG(ERROR) << "Unable to write the router";
            return false;
        }

        LOG(INFO) << "The reset of the storage is set pending";
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool RouterStore::writeConfig(const RouterConfig& router)
{
    if (!router.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = router.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    return writeSetting(kSettingRouter, QString::fromLatin1(encrypted_data->toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool RouterStore::clearConfig()
{
    SqlQuery query(sql(), "DELETE FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingRouter));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}
