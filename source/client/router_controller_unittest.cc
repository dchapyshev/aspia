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

#include "client/router_controller.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router_test_fixture.h"

// The controller without a worker: nothing connects, so what is under test is which records keep
// a context of their own and which lose it.
class RouterControllerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(dir_.isValid());

        file_path_ = dir_.filePath("client.db3");
        DatabaseTestPeer::setFilePath(file_path_);
        ASSERT_TRUE(Database::instance().isValid());

        DataCryptor::instance().setKey(SecureByteArray(QByteArray(32, 'k')));
    }

    qint64 addRouter(const QString& name)
    {
        RouterConfig router;
        router.setDisplayName(name);
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));

        EXPECT_TRUE(Database::instance().addRouter(router));
        return router.routerId();
    }

    bool writeRouter(qint64 router_id, const QString& name)
    {
        RouterConfig router;
        router.setRouterId(router_id);
        router.setDisplayName(name);
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));

        return Database::instance().modifyRouter(router);
    }

    // What a master password entered elsewhere leaves behind: the row is there and its sealed
    // column does not open.
    bool corruptRecordData(qint64 router_id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE routers SET data=X'00' WHERE id=?");
        query.addInt64(router_id);

        return query.exec();
    }

    QTemporaryDir dir_;
    QString file_path_;
};

//--------------------------------------------------------------------------------------------------
// A record that did not open is not a record that is gone, and the user reaches it to enter its
// data again. The login it had cannot finish, so it is given up and the row says the record did
// not open. A record that really left takes its context with it, even though the list it left
// from was not whole.
TEST_F(RouterControllerTest, RecordThatDoesNotOpenGivesUpTheLoginItCannotFinish)
{
    const qint64 kept_id = addRouter("kept");
    const qint64 removed_id = addRouter("removed");

    RouterController controller;
    controller.reload();

    ASSERT_EQ(RouterController::status(kept_id), RouterStatus::CONNECTING);
    ASSERT_EQ(RouterController::status(removed_id), RouterStatus::CONNECTING);

    ASSERT_TRUE(corruptRecordData(kept_id));
    ASSERT_TRUE(Database::instance().removeRouter(removed_id));

    controller.reload();

    EXPECT_EQ(RouterController::status(kept_id), RouterStatus::UNREADABLE);
    EXPECT_EQ(RouterController::status(removed_id), RouterStatus::OFFLINE);

    // Entered again, the record logs in like any other.
    ASSERT_TRUE(writeRouter(kept_id, "kept"));
    controller.reload();

    EXPECT_EQ(RouterController::status(kept_id), RouterStatus::CONNECTING);
}

//--------------------------------------------------------------------------------------------------
// A record that does not open when the controller first sees it has nothing to log in with, so
// it stands in the list as a record that did not open. Entered again, it starts logging in.
TEST_F(RouterControllerTest, RecordThatDoesNotOpenIsNotIdle)
{
    const qint64 router_id = addRouter("broken");
    ASSERT_TRUE(corruptRecordData(router_id));

    RouterController controller;
    controller.reload();

    ASSERT_EQ(RouterController::status(router_id), RouterStatus::UNREADABLE);

    ASSERT_TRUE(writeRouter(router_id, "broken"));

    controller.reload();

    EXPECT_EQ(RouterController::status(router_id), RouterStatus::CONNECTING);
}
