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

#include "client/router/router_controller.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/sql/sql_database.h"
#include "client/config.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/router_store.h"

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

    void addRouter()
    {
        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));

        EXPECT_TRUE(RouterStore().save(router));
    }

    bool writeRouter(bool enabled = true)
    {
        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));
        router.setEnabled(enabled);

        return RouterStore().save(router);
    }

    // What a master password entered elsewhere leaves behind: the row is there and its sealed
    // column does not open.
    bool corruptRecordData()
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        return raw.exec("UPDATE settings SET value='AA==' WHERE name='router'");
    }

    QTemporaryDir dir_;
    QString file_path_;
};

//--------------------------------------------------------------------------------------------------
// A record that did not open is not a record that is gone, and the user reaches it to enter its
// data again. The login it had cannot finish, so it is given up and the row says the record did
// not open.
TEST_F(RouterControllerTest, RecordThatDoesNotOpenGivesUpTheLoginItCannotFinish)
{
    addRouter();

    RouterController controller;
    controller.reload();

    ASSERT_EQ(RouterController::status(), RouterStatus::CONNECTING);

    ASSERT_TRUE(corruptRecordData());
    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::UNREADABLE);

    // Entered again, the record logs in like any other.
    ASSERT_TRUE(writeRouter());
    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::CONNECTING);
}

//--------------------------------------------------------------------------------------------------
// A record that does not open when the controller first sees it has nothing to log in with, so
// it stands in the list as a record that did not open. Entered again, it starts logging in.
TEST_F(RouterControllerTest, RecordThatDoesNotOpenIsNotIdle)
{
    addRouter();
    ASSERT_TRUE(corruptRecordData());

    RouterController controller;
    controller.reload();

    ASSERT_EQ(RouterController::status(), RouterStatus::UNREADABLE);

    ASSERT_TRUE(writeRouter());

    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::CONNECTING);
}

//--------------------------------------------------------------------------------------------------
// A router turned off gives up the login it runs, and turned on again it logs in anew.
TEST_F(RouterControllerTest, RouterTurnedOffGivesUpItsLogin)
{
    addRouter();

    RouterController controller;
    controller.reload();

    ASSERT_EQ(RouterController::status(), RouterStatus::CONNECTING);

    ASSERT_TRUE(writeRouter(false));
    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::OFFLINE);
    EXPECT_EQ(RouterController::session(), nullptr);

    ASSERT_TRUE(writeRouter(true));
    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::CONNECTING);
}

//--------------------------------------------------------------------------------------------------
// A router that is off when the controller first sees it does not log in.
TEST_F(RouterControllerTest, RouterTurnedOffIsNotStarted)
{
    addRouter();
    ASSERT_TRUE(writeRouter(false));

    RouterController controller;
    controller.reload();

    EXPECT_EQ(RouterController::status(), RouterStatus::OFFLINE);
}
