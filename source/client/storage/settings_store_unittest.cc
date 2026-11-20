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

#include "client/storage/settings_store.h"

#include "client/storage/database_test_base.h"

using SettingsStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
TEST_F(SettingsStoreTest, DatabaseHasNoNameUntilItIsGiven)
{
    EXPECT_TRUE(settings_store_.name().isEmpty());
    ASSERT_TRUE(settings_store_.setName("Office"));

    Database reopened;
    ASSERT_TRUE(reopened.open(file_path_));
    EXPECT_EQ(SettingsStore(reopened).name(), QString("Office"));
}

//--------------------------------------------------------------------------------------------------
// Without a choice of the user the application is never locked.
TEST_F(SettingsStoreTest, LockTimeoutIsOffUntilChosen)
{
    EXPECT_EQ(settings_store_.lockTimeout(), Minutes::zero());

    ASSERT_TRUE(settings_store_.setLockTimeout(Minutes(5)));
    EXPECT_EQ(settings_store_.lockTimeout(), Minutes(5));

    ASSERT_TRUE(settings_store_.setLockTimeout(Minutes::zero()));
    EXPECT_EQ(settings_store_.lockTimeout(), Minutes::zero());
}

//--------------------------------------------------------------------------------------------------
// Until the user chooses one, no proxy is used; a choice is kept with all its fields.
TEST_F(SettingsStoreTest, ProxyIsOffUntilChosen)
{
    ProxyConfig proxy;
    ASSERT_EQ(settings_store_.proxyConfig(&proxy), Database::ReadResult::OK);
    EXPECT_FALSE(proxy.settings().enabled);

    ProxySettings settings;
    settings.enabled = true;
    settings.type = ProxySettings::Type::SOCKS5H;
    settings.address = "proxy.example.com";
    settings.port = 1080;
    settings.user = "user";
    settings.password = SecureString("secret");

    proxy.setSettings(settings);
    ASSERT_TRUE(settings_store_.setProxyConfig(proxy));

    ProxyConfig reread;
    ASSERT_EQ(settings_store_.proxyConfig(&reread), Database::ReadResult::OK);
    EXPECT_TRUE(reread.settings().enabled);
    EXPECT_EQ(reread.settings().type, ProxySettings::Type::SOCKS5H);
    EXPECT_EQ(reread.settings().address, QString("proxy.example.com"));
    EXPECT_EQ(reread.settings().port, 1080);
    EXPECT_EQ(reread.settings().user, QString("user"));
    EXPECT_EQ(reread.settings().password.toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(SettingsStoreTest, ProxyIsNotReadAsAbsentWhileTheBaseIsLocked)
{
    ProxySettings settings;
    settings.enabled = true;
    settings.type = ProxySettings::Type::SOCKS5H;
    settings.address = "proxy.example.com";
    settings.port = 1080;

    ProxyConfig proxy;
    proxy.setSettings(settings);
    ASSERT_TRUE(settings_store_.setProxyConfig(proxy));

    SqlDatabase locker;
    ASSERT_TRUE(locker.open(file_path_));

    SqlTransaction transaction(locker);
    ASSERT_TRUE(transaction.begin(SqlTransaction::Mode::EXCLUSIVE));

    ProxyConfig reread;
    EXPECT_EQ(settings_store_.proxyConfig(&reread), Database::ReadResult::FAILED);
    EXPECT_FALSE(reread.settings().enabled);
}
