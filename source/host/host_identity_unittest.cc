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

#include "host/host_identity.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/secure_byte_array.h"
#include "host/database.h"

TEST(HostIdentityTest, KeyIsGeneratedOnceAndKept)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    std::unique_ptr<Database> db = Database::openForTesting(dir.path() + "/host.db3");
    ASSERT_TRUE(db);

    const QByteArray first = HostIdentity::publicKey(*db);
    ASSERT_EQ(first.size(), 32);
    EXPECT_EQ(HostIdentity::publicKey(*db), first);
}

TEST(HostIdentityTest, DamagedKeyIsNotReplacedSilently)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    std::unique_ptr<Database> db = Database::openForTesting(dir.path() + "/host.db3");
    ASSERT_TRUE(db);

    const QByteArray first = HostIdentity::publicKey(*db);
    ASSERT_FALSE(first.isEmpty());

    ASSERT_TRUE(db->setIdentityPrivateKey(QByteArray("damaged")));
    EXPECT_TRUE(HostIdentity::privateKey(*db).isEmpty());
    EXPECT_TRUE(HostIdentity::publicKey(*db).isEmpty());
    // Still damaged: nothing was generated behind the administrator's back.
    EXPECT_EQ(db->identityPrivateKey(), QByteArray("damaged"));

    ASSERT_TRUE(HostIdentity::regenerate(*db));
    const QByteArray second = HostIdentity::publicKey(*db);
    EXPECT_EQ(second.size(), 32);
    EXPECT_NE(second, first);
}
