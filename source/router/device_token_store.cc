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

#include "router/device_token_store.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_constants.h"

namespace {

constexpr qint64 kClientDeviceTokenTtlSec = 7 * 24 * 3600; // 7 days, sliding window.

} // namespace

//--------------------------------------------------------------------------------------------------
DeviceTokenStore::DeviceTokenStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool DeviceTokenStore::issue(qint64 user_id, std::string_view address, std::string* token, qint64* token_id)
{
    CHECK(token);
    *token = std::string();
    if (token_id)
        *token_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // Generate the raw token, hand it back to the caller (it will leave the router to the
    // client) and persist only its SHA-256 hash. Hashing without a salt is safe here because
    // the input is uniformly random CSPRNG output - precomputation/rainbow tables make no
    // sense against 2^256 of entropy.
    std::string new_token = Random::string(proto::router::kDeviceTokenSize);
    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, new_token);
    const qint64 now = secondsSinceEpoch();

    // The cleanup and the insert are one operation: a user must never be left over the cap, and
    // the reply carrying its tokens must never grow past what the channel can send.
    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return false;
    }

    // A device that is never used again leaves behind a row nobody can ever present. Issuing the
    // next token of the same user is the one moment its rows are guaranteed to be touched, so the
    // dead ones go here; without it the table only grows. Only this user is swept: a single login
    // must not turn into a pass over the whole table.
    SqlQuery prune(db(), "DELETE FROM client_device_tokens WHERE user_id=? AND last_used_at < ?");
    prune.addInt64(user_id);
    prune.addInt64(now - kClientDeviceTokenTtlSec);

    if (!prune.exec())
    {
        LOG(ERROR) << "Unable to prune expired client device tokens:" << db().lastError();
        return false;
    }

    // The TTL alone bounds nothing, because a user that logs in from a new device every day keeps
    // every token of the last week alive. The least recently used ones make room for the new one,
    // so the list stays within the cap the protocol counts on.
    const char kTrimSql[] =
        "DELETE FROM client_device_tokens WHERE user_id=? AND token_id NOT IN "
        "(SELECT token_id FROM client_device_tokens WHERE user_id=? "
        "ORDER BY last_used_at DESC, token_id DESC LIMIT ?)";
    SqlQuery trim(db(), kTrimSql);
    trim.addInt64(user_id);
    trim.addInt64(user_id);
    trim.addInt64(proto::router::kMaxDeviceTokensPerUser - 1);

    if (!trim.exec())
    {
        LOG(ERROR) << "Unable to trim client device tokens:" << db().lastError();
        return false;
    }

    const char kSql[] =
        "INSERT INTO client_device_tokens "
        "(token_hash, user_id, created_at, last_used_at, address) VALUES (?, ?, ?, ?, ?)";
    SqlQuery query(db(), kSql);
    query.addBlob(token_hash);
    query.addInt64(user_id);
    query.addInt64(now);
    query.addInt64(now);
    query.addText(address);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to issue client device token:" << db().lastError();
        return false;
    }

    const qint64 new_token_id = db().lastInsertRowId();

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return false;
    }

    if (token_id)
        *token_id = new_token_id;

    *token = std::move(new_token);
    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view DeviceTokenStore::find(std::string_view token, qint64* user_id, qint64* token_id)
{
    CHECK(user_id);
    *user_id = 0;
    if (token_id)
        *token_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (token.size() != proto::router::kDeviceTokenSize)
        return proto::router::kErrorNotFound;

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token);

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    const char kSql[] =
        "SELECT user_id, last_used_at, token_id FROM client_device_tokens WHERE token_hash=?";
    SqlQuery query(db(), kSql);
    query.addBlob(token_hash);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing token, because a missing one makes the
        // client drop its stored copy.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    const qint64 last_used_at = query.columnInt64(1);
    const qint64 now = secondsSinceEpoch();
    if (now - last_used_at > kClientDeviceTokenTtlSec)
    {
        // Lazy GC. The row is dropped so the table does not accumulate stale entries, and the
        // caller sees the same kErrorNotFound an absent token yields.
        SqlQuery prune(db(), "DELETE FROM client_device_tokens WHERE token_hash=?");
        prune.addBlob(token_hash);
        if (!prune.exec())
            LOG(WARNING) << "Unable to prune expired client device token:" << db().lastError();
        else if (!transaction.commit())
            LOG(WARNING) << "Unable to commit transaction:" << db().lastError();
        return proto::router::kErrorNotFound;
    }

    *user_id = query.columnInt64(0);
    if (token_id)
        *token_id = query.columnInt64(2);

    if (!transaction.commit())
        LOG(WARNING) << "Unable to commit transaction:" << db().lastError();

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool DeviceTokenStore::touch(std::string_view token, std::string_view address)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token);

    const char kSql[] =
        "UPDATE client_device_tokens SET last_used_at=?, address=? WHERE token_hash=?";
    SqlQuery query(db(), kSql);
    query.addInt64(secondsSinceEpoch());
    query.addText(address);
    query.addBlob(token_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to touch client device token:" << db().lastError();
        return false;
    }
    return db().changes() > 0;
}

//--------------------------------------------------------------------------------------------------
std::string_view DeviceTokenStore::revoke(qint64 user_id, const std::vector<qint64>& token_ids)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (token_ids.empty())
        return proto::router::kErrorOk;

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    for (qint64 token_id : token_ids)
    {
        SqlQuery query(db(), "DELETE FROM client_device_tokens WHERE token_id=? AND user_id=?");
        query.addInt64(token_id);
        query.addInt64(user_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to revoke client device token:" << db().lastError();
            return proto::router::kErrorInternalError;
        }

        if (db().changes() <= 0)
        {
            LOG(ERROR) << "Device token not found: user_id=" << user_id << "token_id=" << token_id;
            return proto::router::kErrorNotFound;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view DeviceTokenStore::revokeAll(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << user_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing user.
    SqlQuery exists(db(), "SELECT COUNT(*) FROM users WHERE id=?");
    exists.addInt64(user_id);

    if (exists.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check user existence:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    SqlQuery query(db(), "DELETE FROM client_device_tokens WHERE user_id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device tokens:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view DeviceTokenStore::list(qint64 user_id, std::vector<DeviceToken>* tokens) const
{
    CHECK(tokens);

    tokens->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << user_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing user.
    SqlQuery exists(db(), "SELECT COUNT(*) FROM users WHERE id=?");
    exists.addInt64(user_id);

    if (exists.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check user existence:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    // The list answers "which devices can log in as this user without a code", so it must hold
    // exactly what find() would still accept: a token past its lifetime is not a
    // credential any more, and reporting it would show access that nobody has. The boundary is the
    // same one the presentation path uses.
    const char kSql[] =
        "SELECT token_id, created_at, last_used_at, address "
        "FROM client_device_tokens WHERE user_id=? AND last_used_at >= ? ORDER BY created_at";
    SqlQuery query(db(), kSql);
    query.addInt64(user_id);
    query.addInt64(secondsSinceEpoch() - kClientDeviceTokenTtlSec);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            tokens->clear();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        DeviceToken token;
        token.token_id     = query.columnInt64(0);
        token.created_at   = query.columnInt64(1);
        token.last_used_at = query.columnInt64(2);
        token.address      = query.columnTextView(3);
        tokens->emplace_back(std::move(token));
    }

    if (!transaction.commit())
        LOG(WARNING) << "Unable to commit transaction:" << db().lastError();

    return proto::router::kErrorOk;
}
