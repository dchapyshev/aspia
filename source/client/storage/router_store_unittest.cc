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

#include "client/storage/database_test_base.h"
#include "proto/router.h"

namespace {

const char kUserGuid[] = "4f0c1c2e-6a5b-4b0e-9f3a-1d2c3b4a5e6f";
const char kAnotherUserGuid[] = "a1b2c3d4-e5f6-4a7b-8c9d-0e1f2a3b4c5d";

} // namespace

using RouterStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// The form the user edits knows nothing about the guid, so a record built out of it carries none.
// Saving that record must not take the guid away, because the deep links naming the router are held
// by it.
TEST_F(RouterStoreTest, RouterKeepsItsGuidThroughAnEdit)
{
    addRouter();

    const std::optional<RouterConfig> before = findRouter(db_);
    ASSERT_TRUE(before.has_value());
    ASSERT_FALSE(before->guid().isEmpty());

    // What both editors of a router hand over: the fields of the form and nothing else.
    RouterConfig edited;
    edited.setAddress("moved.example.com");
    edited.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    edited.setUsername("router-user");
    edited.setPassword(SecureString("router-secret"));

    ASSERT_TRUE(router_store_.save(edited));

    const std::optional<RouterConfig> after = findRouter(db_);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->address(), "moved.example.com");
    EXPECT_EQ(after->guid(), before->guid());
}

//--------------------------------------------------------------------------------------------------
// A database holds one router, so the one written takes the place of the one there.
TEST_F(RouterStoreTest, DatabaseHoldsOneRouter)
{
    addRouter();

    RouterConfig second;
    second.setAddress("second.example.com");
    second.setUsername("router-user");
    second.setPassword(SecureString("router-secret"));
    EXPECT_TRUE(router_store_.save(second));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->address(), QString("second.example.com"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStoreTest, RouterStaysSwitchedOffUntilSwitchedOn)
{
    addRouter();

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isEnabled());

    stored->setEnabled(false);
    ASSERT_TRUE(router_store_.save(*stored));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isEnabled());
    EXPECT_EQ(stored->address(), QString("router.example.com"));
}

//--------------------------------------------------------------------------------------------------
// The storage and its key are written by the synchronization alone. Whoever saves the record of
// the router holds a copy read some time ago, so what the copy says of the storage is not written:
// the record keeps the storage for as long as the account stays the same, a change of the password
// included.
TEST_F(RouterStoreTest, SyncStorageStaysWhenTheRouterIsSavedForTheSameAccount)
{
    addRouter(true);

    std::optional<RouterConfig> copy = findRouter(db_);
    ASSERT_TRUE(copy.has_value());

    const QByteArray storage_id = Random::byteArray(16);
    const SecureByteArray key(Random::byteArray(32));

    ASSERT_EQ(router_store_.setSyncStorage(*copy, storage_id, key, false), Database::SyncResult::DONE);

    copy->setPassword(SecureString("another-secret"));
    copy->setDeviceToken("token");
    ASSERT_TRUE(router_store_.save(*copy));

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->syncStorageId(), storage_id);
    EXPECT_EQ(stored->syncStorageKey(), key);
    EXPECT_EQ(stored->deviceToken(), QByteArray("token"));

    stored->setSyncStorageId(Random::byteArray(16));
    stored->setSyncStorageKey(SecureByteArray(Random::byteArray(32)));
    ASSERT_TRUE(router_store_.save(*stored));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->syncStorageId(), storage_id);
    EXPECT_EQ(stored->syncStorageKey(), key);
}

//--------------------------------------------------------------------------------------------------
// Whoever edits the record of the router does not know who the user is. A record saved with no
// guid keeps the one the router told at the last sign-in.
TEST_F(RouterStoreTest, GuidOfTheUserIsKeptWhenTheRouterIsSaved)
{
    addRouter();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    EXPECT_TRUE(router->userGuid().isEmpty());

    router->setUserGuid(kUserGuid);
    ASSERT_TRUE(router_store_.save(*router));

    RouterConfig edited = *router;
    edited.setUserGuid(QString());
    edited.setUsername("another-user");
    ASSERT_TRUE(router_store_.save(edited));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("another-user"));
    EXPECT_EQ(stored->userGuid(), QString(kUserGuid));
}

//--------------------------------------------------------------------------------------------------
// Synchronization turned on together with another account waits for the guid of the user that signs in.
TEST_F(RouterStoreTest, SyncTurnedOnForAnotherAccountWaitsForTheUserThatSignsIn)
{
    addRouter();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setUserGuid(kUserGuid);
    ASSERT_TRUE(router_store_.save(*router));

    RouterConfig edited = *router;
    edited.setUserGuid(QString());
    edited.setUsername("another-user");
    edited.setSyncEnabled(true);
    ASSERT_TRUE(router_store_.save(edited));

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_TRUE(stored->userGuid().isEmpty());

    stored->setUserGuid(kAnotherUserGuid);
    ASSERT_TRUE(router_store_.save(*stored));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_EQ(stored->userGuid(), QString(kAnotherUserGuid));
}

//--------------------------------------------------------------------------------------------------
// Without a record of the router there is no account for a storage to belong to.
TEST_F(RouterStoreTest, SyncStorageIsNotSetWithoutTheRouter)
{
    EXPECT_EQ(router_store_.setSyncStorage(RouterConfig(), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DETACHED);
    EXPECT_FALSE(findRouter(db_).has_value());
}

//--------------------------------------------------------------------------------------------------
// A device with the synchronization turned off is attached to no storage, so none is written.
TEST_F(RouterStoreTest, SyncStorageIsNotSetWhileSyncIsOff)
{
    addRouter();

    const std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    EXPECT_EQ(router_store_.setSyncStorage(*router, Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DETACHED);

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->syncStorageId().isEmpty());
    EXPECT_TRUE(stored->syncStorageKey().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The synchronization writes the storage for the record it was started with. Once the record is
// saved for another account, a copy read before that does not write the storage, and the record
// keeps what it had.
TEST_F(RouterStoreTest, SyncStorageIsNotSetFromACopyOfAnotherAccount)
{
    addRouter(true);

    const std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    const QByteArray storage_id = Random::byteArray(16);
    const SecureByteArray key(Random::byteArray(32));
    ASSERT_EQ(router_store_.setSyncStorage(*router, storage_id, key, false), Database::SyncResult::DONE);

    RouterConfig another = *router;
    another.setUsername("another-user");
    ASSERT_TRUE(router_store_.save(another));

    EXPECT_EQ(router_store_.setSyncStorage(*router, Random::byteArray(16), key, false), Database::SyncResult::DETACHED);

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("another-user"));
    EXPECT_EQ(stored->syncStorageId(), storage_id);
    EXPECT_EQ(stored->syncStorageKey(), key);
}

//--------------------------------------------------------------------------------------------------
// The synchronization is off until it is turned on, and the choice is kept in the record of the
// router.
TEST_F(RouterStoreTest, SyncIsOffUntilTurnedOn)
{
    addRouter();

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncEnabled());

    stored->setSyncEnabled(true);
    ASSERT_TRUE(router_store_.save(*stored));

    const std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    EXPECT_TRUE(router->isSyncEnabled());
}

//--------------------------------------------------------------------------------------------------
// A router is entered with an account of its own, so one without a password could never be
// connected to. What the readers of the address book call invalid does not get in.
TEST_F(RouterStoreTest, RouterWithoutAPasswordIsNotStored)
{
    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");

    EXPECT_FALSE(router_store_.save(router));
    EXPECT_FALSE(findRouter(db_).has_value());
}

//--------------------------------------------------------------------------------------------------
// A write over the router there cannot leave behind one without a password.
TEST_F(RouterStoreTest, RouterCannotBeEditedIntoOneWithoutAPassword)
{
    addRouter();

    RouterConfig edited;
    edited.setAddress("moved.example.com");
    edited.setUsername("router-user");

    EXPECT_FALSE(router_store_.save(edited));

    const std::optional<RouterConfig> after = findRouter(db_);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->address(), "router.example.com");
    EXPECT_FALSE(after->password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A database without a router is given one by the same write that edits a router already there.
TEST_F(RouterStoreTest, RouterIsWrittenWhereNoneIsSet)
{
    ASSERT_FALSE(findRouter(db_).has_value());

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    router.setUsername("router-user");
    router.setPassword(SecureString("router-secret"));

    EXPECT_TRUE(router_store_.save(router));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->address(), QString("router.example.com"));
}

//--------------------------------------------------------------------------------------------------
// The pending reset is written by the synchronization alone. Whoever saves the record of the
// router holds a copy read some time ago, so what the copy says of the reset is not written.
TEST_F(RouterStoreTest, PendingResetIsKeptWhenTheRouterIsSaved)
{
    addRouter(true);

    std::optional<RouterConfig> copy = findRouter(db_);
    ASSERT_TRUE(copy.has_value());

    ASSERT_TRUE(router_store_.setSyncStorageResetPending(*copy));

    copy->setPassword(SecureString("another-secret"));
    ASSERT_TRUE(router_store_.save(*copy));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncStorageResetPending());
    EXPECT_EQ(stored->password().toString(), QString("another-secret"));
}

//--------------------------------------------------------------------------------------------------
// Only an attachment made with the reset asked for drops the pending reset.
TEST_F(RouterStoreTest, AttachWithoutResetKeepsResetPending)
{
    attachSyncStorage();

    const std::optional<RouterConfig> snapshot = findRouter(db_);
    ASSERT_TRUE(snapshot.has_value());
    ASSERT_FALSE(snapshot->isSyncStorageResetPending());

    ASSERT_TRUE(router_store_.setSyncStorageResetPending(*snapshot));
    ASSERT_EQ(router_store_.setSyncStorage(*snapshot, snapshot->syncStorageId(), snapshot->syncStorageKey(), false),
              Database::SyncResult::DONE);

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncStorageResetPending());

    ASSERT_EQ(router_store_.setSyncStorage(*stored, Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
// The pending reset is kept together with the storage. A record saved for another name keeps it,
// and a record saved with the synchronization turned off drops it.
TEST_F(RouterStoreTest, PendingResetIsDroppedWhenSyncIsTurnedOff)
{
    addRouter(true);
    ASSERT_TRUE(router_store_.setSyncStorageResetPending(*findRouter(db_)));

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    ASSERT_TRUE(router->isSyncStorageResetPending());

    router->setUsername("another-user");
    ASSERT_TRUE(router_store_.save(*router));

    router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    EXPECT_TRUE(router->isSyncStorageResetPending());

    router->setSyncEnabled(false);
    ASSERT_TRUE(router_store_.save(*router));

    router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    EXPECT_FALSE(router->isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
// A device with the synchronization turned off, or with a record of another account, has no
// storage to reset.
TEST_F(RouterStoreTest, PendingResetIsNotSetWhileSyncIsOff)
{
    EXPECT_FALSE(router_store_.setSyncStorageResetPending(RouterConfig()));

    addRouter();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    EXPECT_FALSE(router_store_.setSyncStorageResetPending(*router));

    router->setSyncEnabled(true);
    ASSERT_TRUE(router_store_.save(*router));

    RouterConfig another = *router;
    another.setUsername("another-user");
    EXPECT_FALSE(router_store_.setSyncStorageResetPending(another));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncStorageResetPending());
}
