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

#include "host/database.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/password_generator.h"
#include "base/crypto/secure_string.h"
#include "build/build_config.h"

namespace {

const SecureString kPassword("s3cret-password");

} // namespace

// The secure storage of the host against a real database in a temporary directory. Nothing here
// touches the machine-wide file the service uses.
class HostDatabaseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());

        db_ = Database::openForTesting(temp_dir_.path() + "/host.db3");
        ASSERT_TRUE(db_);
        ASSERT_TRUE(db_->isValid());
    }

    static User user(const QString& name)
    {
        return User::create(name, SecureString("password"));
    }

    QTemporaryDir temp_dir_;
    std::unique_ptr<Database> db_;
};

//--------------------------------------------------------------------------------------------------
// A user comes back as it was stored and is found both by name and by the id it was given.
TEST_F(HostDatabaseTest, StoredUserIsFoundByNameAndById)
{
    const User stored = user("john");
    ASSERT_TRUE(db_->addUser(stored));

    const User by_name = db_->findUser(QString("john"));
    ASSERT_TRUE(by_name.isValid());
    EXPECT_EQ(by_name.name, stored.name);
    EXPECT_EQ(by_name.salt, stored.salt);
    EXPECT_EQ(by_name.verifier, stored.verifier);
    EXPECT_GT(by_name.entry_id, 0);

    const User by_id = db_->findUser(by_name.entry_id);
    ASSERT_TRUE(by_id.isValid());
    EXPECT_EQ(by_id.name, stored.name);
}

//--------------------------------------------------------------------------------------------------
// Names are what the peer authenticates with, so two users cannot share one.
TEST_F(HostDatabaseTest, UserNameIsTaken)
{
    ASSERT_TRUE(db_->addUser(user("john")));

    EXPECT_FALSE(db_->addUser(user("john")));
    EXPECT_EQ(db_->userList().size(), 1);
}

//--------------------------------------------------------------------------------------------------
// SRP folds the user name to lower case before it derives the verifier, so records differing only
// in case would be one identity with two passwords. The database refuses the second one and finds
// the first whatever case the peer connects with.
TEST_F(HostDatabaseTest, UserNamesAreCaseFolded)
{
    ASSERT_TRUE(db_->addUser(user("john")));
    EXPECT_FALSE(db_->addUser(user("JoHn")));

    const User found = db_->findUser(QString("JOHN"));
    ASSERT_TRUE(found.isValid());
    EXPECT_EQ(found.name, "john");

    // A rename cannot take the folded name of somebody else either.
    ASSERT_TRUE(db_->addUser(user("mary")));

    User mary = db_->findUser(QString("mary"));
    ASSERT_TRUE(mary.isValid());
    mary.name = "JOHN";
    EXPECT_FALSE(db_->modifyUser(mary));
}

//--------------------------------------------------------------------------------------------------
// A user without a name or without a verifier is not a user, and the storage refuses it instead of
// keeping a record nobody can authenticate against.
TEST_F(HostDatabaseTest, IncompleteUserIsRefused)
{
    EXPECT_FALSE(db_->addUser(User()));
    EXPECT_FALSE(db_->modifyUser(User()));
    EXPECT_TRUE(db_->userList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An edit reaches the record the id points at and leaves the rest alone.
TEST_F(HostDatabaseTest, UserIsModifiedInPlace)
{
    ASSERT_TRUE(db_->addUser(user("john")));
    ASSERT_TRUE(db_->addUser(user("mary")));

    User john = db_->findUser(QString("john"));
    ASSERT_TRUE(john.isValid());

    john.sessions = 3;
    john.flags = User::ENABLED;
    ASSERT_TRUE(db_->modifyUser(john));

    const User reread = db_->findUser(john.entry_id);
    ASSERT_TRUE(reread.isValid());
    EXPECT_EQ(reread.sessions, 3u);
    EXPECT_EQ(reread.flags, static_cast<quint32>(User::ENABLED));

    EXPECT_EQ(db_->findUser(QString("mary")).sessions, 0u);
}

//--------------------------------------------------------------------------------------------------
// A removed user cannot authenticate anymore.
TEST_F(HostDatabaseTest, RemovedUserIsGone)
{
    ASSERT_TRUE(db_->addUser(user("john")));
    const User john = db_->findUser(QString("john"));
    ASSERT_TRUE(john.isValid());

    EXPECT_TRUE(db_->removeUser(john.entry_id));
    EXPECT_FALSE(db_->findUser(QString("john")).isValid());
    EXPECT_TRUE(db_->userList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The import replaces the whole list at once.
TEST_F(HostDatabaseTest, ReplaceUsersSwapsTheWholeList)
{
    ASSERT_TRUE(db_->addUser(user("john")));

    QVector<User> users;
    users.append(user("mary"));
    users.append(user("paul"));

    ASSERT_TRUE(db_->replaceUsers(users));

    EXPECT_EQ(db_->userList().size(), 2);
    EXPECT_FALSE(db_->findUser(QString("john")).isValid());
    EXPECT_TRUE(db_->findUser(QString("mary")).isValid());
    EXPECT_TRUE(db_->findUser(QString("paul")).isValid());
}

//--------------------------------------------------------------------------------------------------
// An import that cannot be carried out to the end leaves the users that were there. A half applied
// import is a host nobody can connect to.
TEST_F(HostDatabaseTest, FailedReplaceKeepsTheStoredUsers)
{
    ASSERT_TRUE(db_->addUser(user("john")));

    QVector<User> users;
    users.append(user("mary"));
    users.append(User());

    EXPECT_FALSE(db_->replaceUsers(users));

    ASSERT_EQ(db_->userList().size(), 1);
    EXPECT_TRUE(db_->findUser(QString("john")).isValid());
}

//--------------------------------------------------------------------------------------------------
// A fresh database answers with the defaults of the host instead of empty values.
TEST_F(HostDatabaseTest, FreshDatabaseAnswersWithDefaults)
{
    EXPECT_EQ(db_->tcpPort(), DEFAULT_HOST_TCP_PORT);
    EXPECT_FALSE(db_->isRouterEnabled());
    EXPECT_TRUE(db_->oneTimePassword());
    EXPECT_EQ(db_->oneTimePasswordExpire(), Minutes(5));
    EXPECT_EQ(db_->oneTimePasswordLength(), 8);
    EXPECT_EQ(db_->autoConfirmationInterval(), MilliSeconds(0));
    EXPECT_EQ(db_->passwordProtectionState(), Database::PasswordProtection::DISABLED);
    EXPECT_TRUE(db_->seedKey().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Settings come back as they were written, including the ones stored as hex.
TEST_F(HostDatabaseTest, SettingsSurviveAWriteAndRead)
{
    Address address(DEFAULT_ROUTER_HOST_TCP_PORT);
    address.setHost("router.example.com");
    address.setPort(8061);

    // Both are stored as hex, so they are binary on purpose: zero bytes and everything above the
    // ascii range have to survive the trip.
    const QByteArray key("\x00\x01\xfe\xff public key", 15);
    const QByteArray seed("\x00\xaa\xbb seed", 8);

    ASSERT_TRUE(db_->setTcpPort(9999));
    ASSERT_TRUE(db_->setRouterEnabled(true));
    ASSERT_TRUE(db_->setRouterAddress(address));
    ASSERT_TRUE(db_->setRouterPublicKey(key));
    ASSERT_TRUE(db_->setSeedKey(seed));
    ASSERT_TRUE(db_->setConnectConfirmation(true));
    ASSERT_TRUE(db_->setNoUserAction(Database::NoUserAction::ACCEPT));

    EXPECT_EQ(db_->tcpPort(), 9999);
    EXPECT_TRUE(db_->isRouterEnabled());
    EXPECT_EQ(db_->routerAddress(), address);
    EXPECT_EQ(db_->routerPublicKey(), key);
    EXPECT_EQ(db_->seedKey(), seed);
    EXPECT_TRUE(db_->connectConfirmation());
    EXPECT_EQ(db_->noUserAction(), Database::NoUserAction::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// The values that drive the one-time password are read back inside the bounds the host works with,
// whatever ended up in the file.
TEST_F(HostDatabaseTest, OneTimePasswordSettingsAreKeptInBounds)
{
    ASSERT_TRUE(db_->setOneTimePasswordExpire(Hours(48)));
    EXPECT_EQ(db_->oneTimePasswordExpire(), Hours(24));

    ASSERT_TRUE(db_->setOneTimePasswordExpire(MilliSeconds(-1)));
    EXPECT_EQ(db_->oneTimePasswordExpire(), MilliSeconds(0));

    ASSERT_TRUE(db_->setOneTimePasswordLength(2));
    EXPECT_EQ(db_->oneTimePasswordLength(), 6);

    ASSERT_TRUE(db_->setOneTimePasswordLength(100));
    EXPECT_EQ(db_->oneTimePasswordLength(), 16);

    // A password of no characters at all cannot be generated, so the stored value gives way to the
    // default set.
    ASSERT_TRUE(db_->setOneTimePasswordCharacters(0));
    EXPECT_NE(db_->oneTimePasswordCharacters(), 0u);

    ASSERT_TRUE(db_->setOneTimePasswordCharacters(PasswordGenerator::DIGITS));
    EXPECT_EQ(db_->oneTimePasswordCharacters(),
              static_cast<quint32>(PasswordGenerator::DIGITS));

    ASSERT_TRUE(db_->setAutoConfirmationInterval(Minutes(5)));
    EXPECT_EQ(db_->autoConfirmationInterval(), Seconds(60));
}

//--------------------------------------------------------------------------------------------------
// The password that guards the settings is stored as a hash and verified against it.
TEST_F(HostDatabaseTest, PasswordProtectionIsSetAndVerified)
{
    ASSERT_TRUE(db_->setPassword(kPassword));
    EXPECT_EQ(db_->passwordProtectionState(), Database::PasswordProtection::ENABLED);

    EXPECT_TRUE(db_->verifyPassword(kPassword));
    EXPECT_FALSE(db_->verifyPassword(SecureString("s3cret-passwor")));
    EXPECT_FALSE(db_->verifyPassword(SecureString()));
}

//--------------------------------------------------------------------------------------------------
// Clearing the protection leaves nothing that could still be verified against.
TEST_F(HostDatabaseTest, ClearedPasswordVerifiesAgainstNothing)
{
    ASSERT_TRUE(db_->setPassword(kPassword));

    db_->clearPassword();

    EXPECT_EQ(db_->passwordProtectionState(), Database::PasswordProtection::DISABLED);
    EXPECT_FALSE(db_->verifyPassword(kPassword));
}

//--------------------------------------------------------------------------------------------------
// An empty password would protect nothing, so it is refused and what was there stays.
TEST_F(HostDatabaseTest, EmptyPasswordIsRefused)
{
    ASSERT_TRUE(db_->setPassword(kPassword));

    EXPECT_FALSE(db_->setPassword(SecureString()));

    EXPECT_EQ(db_->passwordProtectionState(), Database::PasswordProtection::ENABLED);
    EXPECT_TRUE(db_->verifyPassword(kPassword));
}

//--------------------------------------------------------------------------------------------------
// The same password gets its own salt every time, so two hosts with one password do not share a
// hash.
TEST_F(HostDatabaseTest, PasswordIsSaltedAnew)
{
    ASSERT_TRUE(db_->setPassword(kPassword));

    std::unique_ptr<Database> other = Database::openForTesting(temp_dir_.path() + "/other.db3");
    ASSERT_TRUE(other);
    ASSERT_TRUE(other->setPassword(kPassword));

    EXPECT_TRUE(other->verifyPassword(kPassword));
}

//--------------------------------------------------------------------------------------------------
// What was written stays in the file and is there for the next connection to the same database.
TEST_F(HostDatabaseTest, ContentsSurviveReopening)
{
    ASSERT_TRUE(db_->addUser(user("john")));
    ASSERT_TRUE(db_->setTcpPort(9999));
    ASSERT_TRUE(db_->setPassword(kPassword));

    db_.reset();

    std::unique_ptr<Database> reopened = Database::openForTesting(temp_dir_.path() + "/host.db3");
    ASSERT_TRUE(reopened);

    EXPECT_TRUE(reopened->findUser(QString("john")).isValid());
    EXPECT_EQ(reopened->tcpPort(), 9999);
    EXPECT_TRUE(reopened->verifyPassword(kPassword));
}
