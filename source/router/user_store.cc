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

#include "router/user_store.h"

#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router.h"
#include "proto/router_constants.h"

namespace {

// The user created by --create-config always gets id 1 (AUTOINCREMENT on an empty table). It is the
// router's guaranteed way into the admin channel, because the router has no CLI to restore
// administrator access, so losing it means losing control over the installation. The record is
// therefore not deletable, must stay enabled and keeps the administrator level, since a disabled or
// demoted account would lock everyone out of that channel.
constexpr qint64 kBuiltInUserId = 1;

//--------------------------------------------------------------------------------------------------
// Reads a RouterUser row. Column order must match the SELECT projections used by list() and find().
RouterUser readUser(const SqlQuery& query)
{
    RouterUser user;
    user.entry_id    = query.columnInt64(0);
    user.name        = query.columnText(1);
    user.group       = query.columnText(2);
    user.salt        = query.columnBlob(3);
    user.verifier    = query.columnBlob(4);
    user.sessions    = static_cast<quint32>(query.columnInt64(5));
    user.flags       = static_cast<quint32>(query.columnInt64(6));
    user.otp_secret  = query.columnBlob(7);
    user.otp_counter = query.columnUInt64(8);
    user.email       = query.columnText(9);
    user.guid        = query.columnText(10);
    return user;
}

} // namespace

//--------------------------------------------------------------------------------------------------
UserStore::UserStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::list(qint64 offset, qint64 count, std::vector<RouterUser>* users) const
{
    CHECK(users);

    users->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!isPageValid(offset, count, proto::router::kMaxUserPageSize))
    {
        LOG(ERROR) << "Invalid user list page: offset" << offset << "count" << count;
        return proto::router::kErrorInvalidRequest;
    }

    // The page has to name the same records every time it is asked for, so the order is fixed
    // here instead of being left to the query planner.
    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, otp_secret, otp_counter, email, guid "
        "FROM users ORDER BY id LIMIT ? OFFSET ?";
    SqlQuery query(db(), kSql);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            users->clear();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        users->emplace_back(readUser(query));
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::count(qint64* count) const
{
    CHECK(count);

    *count = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so anything but ROW is a database error - it must
    // not pass for an empty table.
    SqlQuery query(db(), "SELECT COUNT(*) FROM users");
    if (query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    *count = query.columnInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::add(const RouterUser& user)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    // Checked here instead of relying on the UNIQUE constraint: the INSERT failure below cannot
    // be told apart from a real database error, so a lost create race would answer with a
    // misleading internal error.
    SqlQuery name_check(db(), "SELECT 1 FROM users WHERE casefold(name)=casefold(?)");
    name_check.addText(user.name);

    const SqlQuery::StepResult name_step = name_check.next();
    if (name_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (name_step == SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "User name already exists:" << user.name;
        return proto::router::kErrorAlreadyExists;
    }

    const char kSql[] =
        "INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags, email, guid) "
        "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?)";
    SqlQuery query(db(), kSql);
    query.addText(user.name);
    query.addText(user.group);
    query.addBlob(user.salt);
    query.addBlob(user.verifier);
    query.addInt64(RouterUser::expandSessionTypes(user.sessions));
    query.addInt64(user.flags);
    query.addText(user.email);
    query.addText(QUuid::createUuid().toString(QUuid::WithoutBraces));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
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
std::string_view UserStore::modify(const RouterUser& user, bool* password_changed, bool* sessions_changed)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    // A request with empty credentials does not change them: the stored values are kept and the
    // rotation branch is skipped. The user dialog sends such a request when only the properties
    // of the record are edited, so a snapshot taken before a concurrent password change cannot
    // roll that change back.
    const bool has_credentials = !user.salt.isEmpty() || !user.verifier.isEmpty();
    if (has_credentials && !user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return proto::router::kErrorInvalidData;
    }

    if (user.entry_id == kBuiltInUserId)
    {
        if (!(user.flags & User::ENABLED))
        {
            LOG(ERROR) << "Attempt to disable the built-in user";
            return proto::router::kErrorAccessDenied;
        }

        if (user.sessions && !(user.sessions & proto::router::SESSION_TYPE_ADMIN))
        {
            LOG(ERROR) << "Attempt to demote the built-in user";
            return proto::router::kErrorAccessDenied;
        }
    }

    if (password_changed)
        *password_changed = false;
    if (sessions_changed)
        *sessions_changed = false;

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    // Read the existing verifier first - a change to salt or verifier means a password rotation,
    // which must invalidate every device token.
    QByteArray old_salt;
    QByteArray old_verifier;
    quint32 old_sessions = 0;
    QString old_email;
    bool user_found = false;
    {
        SqlQuery select(db(), "SELECT salt, verifier, sessions, email FROM users WHERE id=?");
        select.addInt64(user.entry_id);

        if (!select.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }

        const SqlQuery::StepResult step = select.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed read must not pass for a missing user: the caller would report NotFound
            // for a row that is still there.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            user_found = true;
            old_salt = select.columnBlob(0);
            old_verifier = select.columnBlob(1);
            old_sessions = static_cast<quint32>(select.columnInt64(2));
            old_email = select.columnText(3);
        }
    }

    if (!user_found)
        return proto::router::kErrorNotFound;

    const quint32 sessions =
        user.sessions ? RouterUser::expandSessionTypes(user.sessions) : old_sessions;
    const QString email = user.email.isEmpty() ? old_email : user.email;

    if (has_credentials)
    {
        // Same reasoning as in add(): a rename that lost a race must answer
        // kErrorAlreadyExists, not the internal error of the UNIQUE constraint.
        SqlQuery name_check(db(), "SELECT 1 FROM users WHERE casefold(name)=casefold(?) AND id!=?");
        name_check.addText(user.name);
        name_check.addInt64(user.entry_id);

        const SqlQuery::StepResult name_step = name_check.next();
        if (name_step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }

        if (name_step == SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "User name already exists:" << user.name;
            return proto::router::kErrorAlreadyExists;
        }

        const char kSql[] =
            "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=?, email=? "
            "WHERE id=?";
        SqlQuery query(db(), kSql);
        query.addText(user.name);
        query.addText(user.group);
        query.addBlob(user.salt);
        query.addBlob(user.verifier);
        query.addInt64(sessions);
        query.addInt64(user.flags);
        query.addText(email);
        query.addInt64(user.entry_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }
    }
    else
    {
        SqlQuery query(db(), "UPDATE users SET sessions=?, flags=?, email=? WHERE id=?");
        query.addInt64(sessions);
        query.addInt64(user.flags);
        query.addText(email);
        query.addInt64(user.entry_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }
    }

    const bool rotated = has_credentials &&
        (old_salt != user.salt || old_verifier != user.verifier);
    if (password_changed)
        *password_changed = rotated;
    if (sessions_changed)
        *sessions_changed = sessions != old_sessions;

    if (rotated)
    {
        SqlQuery revoke(db(), "DELETE FROM client_device_tokens WHERE user_id=?");
        revoke.addInt64(user.entry_id);

        if (!revoke.exec())
        {
            LOG(ERROR) << "Unable to revoke device tokens:" << db().lastError();
            return proto::router::kErrorInternalError;
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
std::string_view UserStore::remove(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    if (entry_id == kBuiltInUserId)
    {
        LOG(ERROR) << "Attempt to delete the built-in user";
        return proto::router::kErrorAccessDenied;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    // The cascade of the DELETE removes the user's workspace_access rows, which changes the
    // membership of those workspaces: a save built from a member list that still contains the
    // user must get kErrorConflict instead of re-adding it, so the revisions move together
    // with the delete.
    SqlQuery bump(db(),
        "UPDATE workspaces SET revision=revision+1 WHERE id IN "
        "(SELECT workspace_id FROM workspace_access WHERE user_id=?)");
    bump.addInt64(entry_id);

    if (!bump.exec())
    {
        LOG(ERROR) << "Unable to update workspace revisions:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "DELETE FROM users WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (db().changes() == 0)
        return proto::router::kErrorNotFound;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::find(const QString& username, RouterUser* user) const
{
    CHECK(user);

    *user = RouterUser();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, otp_secret, otp_counter, email, guid "
        "FROM users WHERE casefold(name)=casefold(?)";
    SqlQuery query(db(), kSql);
    query.addText(username);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing user: a caller that reports "no such name"
        // would let a duplicate through, and one that lists a single record would answer an
        // empty list for a record that is there.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *user = readUser(query);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::find(qint64 entry_id, RouterUser* user) const
{
    CHECK(user);

    *user = RouterUser();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, otp_secret, otp_counter, email, guid "
        "FROM users WHERE id=?";
    SqlQuery query(db(), kSql);
    query.addInt64(entry_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // See the overload above: a failed read is not a missing record.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *user = readUser(query);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::findByGuid(const QString& guid, RouterUser* user) const
{
    CHECK(user);

    *user = RouterUser();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, otp_secret, otp_counter, email, guid "
        "FROM users WHERE guid=?";
    SqlQuery query(db(), kSql);
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *user = readUser(query);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool UserStore::setOtp(qint64 user_id, const QByteArray& secret, quint64 counter)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE users SET otp_secret=?, otp_counter=? WHERE id=? AND otp_secret=X''");
    query.addBlob(secret);
    query.addUInt64(counter);
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to set user OTP:" << db().lastError();
        return false;
    }
    return db().changes() > 0;
}

//--------------------------------------------------------------------------------------------------
std::string_view UserStore::resetOtp(qint64 user_id)
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

    SqlQuery query(db(), "UPDATE users SET otp_secret=X'', otp_counter=0 WHERE id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to clear user OTP:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery revoke(db(), "DELETE FROM client_device_tokens WHERE user_id=?");
    revoke.addInt64(user_id);

    if (!revoke.exec())
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
std::string_view UserStore::consumeOtpCounter(qint64 user_id, quint64 counter)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "UPDATE users SET otp_counter=? WHERE id=? AND otp_counter < ?");
    query.addUInt64(counter);
    query.addInt64(user_id);
    query.addUInt64(counter);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to consume OTP counter:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (db().changes() == 0)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}
