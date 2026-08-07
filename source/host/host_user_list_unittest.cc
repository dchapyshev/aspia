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

#include "host/host_user_list.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/secure_string.h"
#include "host/database.h"

// What the authenticator of the host sees when a peer names itself: the stored users and the
// one-time user that only lives while the router hands out an id for it.
class HostUserListTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());

        db_ = Database::openForTesting(temp_dir_.path() + "/host.db3");
        ASSERT_TRUE(db_);

        list_ = std::make_unique<HostUserList>(*db_);
    }

    static User user(const QString& name, const QString& password)
    {
        return User::create(name, SecureString(password));
    }

    QTemporaryDir temp_dir_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<HostUserList> list_;
};

//--------------------------------------------------------------------------------------------------
// A stored user is found with the material the authenticator needs.
TEST_F(HostUserListTest, StoredUserIsFound)
{
    ASSERT_TRUE(db_->addUser(user("john", "password")));

    const User found = list_->find("john");
    ASSERT_TRUE(found.isValid());
    EXPECT_EQ(found.name, QString("john"));
    EXPECT_FALSE(found.salt.isEmpty());
    EXPECT_FALSE(found.verifier.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A name nobody has is not somebody to authenticate.
TEST_F(HostUserListTest, UnknownNameIsNotFound)
{
    EXPECT_FALSE(list_->find("john").isValid());
}

//--------------------------------------------------------------------------------------------------
// The one-time user is not in the database and is still found, which is what makes the one-time
// password work.
TEST_F(HostUserListTest, OneTimeUserIsFound)
{
    list_->setOneTimeUser(user("#123456", "one-time"));

    const User found = list_->find("#123456");
    ASSERT_TRUE(found.isValid());
    EXPECT_EQ(found.name, QString("#123456"));
    EXPECT_FALSE(db_->findUser(QString("#123456")).isValid());
}

//--------------------------------------------------------------------------------------------------
// The user types the name and the case of it is not part of what they were told.
TEST_F(HostUserListTest, OneTimeUserIsFoundWhateverTheCase)
{
    list_->setOneTimeUser(user("#abc", "one-time"));

    EXPECT_TRUE(list_->find("#ABC").isValid());
    EXPECT_TRUE(list_->find("#Abc").isValid());
}

//--------------------------------------------------------------------------------------------------
// While there is no id from the router there is no one-time user either, so the name it used to
// answer to authenticates nobody.
TEST_F(HostUserListTest, ClearedOneTimeUserIsNotFound)
{
    list_->setOneTimeUser(user("#123456", "one-time"));
    ASSERT_TRUE(list_->find("#123456").isValid());

    list_->setOneTimeUser(User());

    EXPECT_FALSE(list_->find("#123456").isValid());
}

//--------------------------------------------------------------------------------------------------
// A one-time user replaces the previous one instead of adding to it: only the credentials the user
// is being shown right now open a session.
TEST_F(HostUserListTest, OneTimeUserReplacesThePreviousOne)
{
    list_->setOneTimeUser(user("#111", "one-time"));
    list_->setOneTimeUser(user("#222", "one-time"));

    EXPECT_FALSE(list_->find("#111").isValid());
    EXPECT_TRUE(list_->find("#222").isValid());
}

//--------------------------------------------------------------------------------------------------
// A stored user of that name answers first. The administrator of the host decides who its users
// are, and a one-time name must not take a stored name over.
TEST_F(HostUserListTest, StoredUserWinsOverTheOneTimeName)
{
    const User stored = user("#123456", "stored");
    ASSERT_TRUE(db_->addUser(stored));

    list_->setOneTimeUser(user("#123456", "one-time"));

    const User found = list_->find("#123456");
    ASSERT_TRUE(found.isValid());
    EXPECT_EQ(found.verifier, stored.verifier);
}

//--------------------------------------------------------------------------------------------------
// The seed key of the authenticator is kept in the database, so it is the same one after a restart.
TEST_F(HostUserListTest, SeedKeyGoesThroughTheDatabase)
{
    const QByteArray seed("\x00\x11\x22 seed", 7);

    list_->setSeedKey(seed);

    EXPECT_EQ(db_->seedKey(), seed);
    EXPECT_EQ(list_->seedKey(), seed);
}
