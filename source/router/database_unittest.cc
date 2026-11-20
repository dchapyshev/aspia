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

#include "router/database.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QUuid>

#include "base/crypto/secure_string.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "router/user_store.h"

namespace {

constexpr quint32 kAllSessions = proto::router::SESSION_TYPE_ADMIN |
    proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_OPERATOR;

//--------------------------------------------------------------------------------------------------
RouterUser makeUser(const QString& name, quint32 sessions)
{
    RouterUser user = RouterUser::create(name, SecureString("Password1234!"));
    user.sessions = sessions;
    user.flags = User::ENABLED;
    return user;
}

} // namespace

// Every test opens an isolated database in a temporary directory and starts from the state
// --create-config leaves behind: the built-in administrator with id 1.
class RouterDatabaseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/router.db3";
        ASSERT_TRUE(db_.open(file_path_));

        UserStore store(db_);

        admin_ = makeUser("admin", kAllSessions);
        ASSERT_TRUE(admin_.isValid());
        ASSERT_EQ(store.add(admin_), proto::router::kErrorOk);

        ASSERT_EQ(store.find("admin", &admin_), proto::router::kErrorOk);
        ASSERT_EQ(admin_.entry_id, 1);
    }

    // Doctors rows the public API deliberately cannot produce (legacy states, induced holes).
    bool execRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;
        return raw.exec(sql.toStdString().c_str());
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
    Database db_;
    RouterUser admin_;
};

//--------------------------------------------------------------------------------------------------
// The users table of 3.4.2 has no guid column. Opening such a database adds it and gives every
// user a guid of its own.
TEST_F(RouterDatabaseTest, UsersTableOf342GainsTheGuidColumn)
{
    ASSERT_EQ(UserStore(db_).add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);
    ASSERT_TRUE(execRaw("ALTER TABLE users DROP COLUMN guid"));
    ASSERT_TRUE(execRaw("PRAGMA user_version = 0"));

    Database reopened;
    ASSERT_TRUE(reopened.open(file_path_));

    UserStore reopened_store(reopened);

    RouterUser admin;
    ASSERT_EQ(reopened_store.find(admin_.entry_id, &admin), proto::router::kErrorOk);
    EXPECT_FALSE(QUuid::fromString(admin.guid).isNull());

    RouterUser bob;
    ASSERT_EQ(reopened_store.find("bob", &bob), proto::router::kErrorOk);
    EXPECT_FALSE(QUuid::fromString(bob.guid).isNull());
    EXPECT_NE(bob.guid, admin.guid);

    Database opened_again;
    ASSERT_TRUE(opened_again.open(file_path_));

    RouterUser unchanged;
    ASSERT_EQ(UserStore(opened_again).find("bob", &unchanged), proto::router::kErrorOk);
    EXPECT_EQ(unchanged.guid, bob.guid);

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path_));

    SqlQuery version(raw, "PRAGMA user_version");
    ASSERT_EQ(version.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(version.columnInt64(0), 1);
}
