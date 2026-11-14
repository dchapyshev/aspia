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

#include "client/storage/user_settings.h"

#include <QSettings>

#include <gtest/gtest.h>

#include "base/build_config.h"
#include "proto/peer.h"

namespace {

class UserSettingsTest : public testing::Test
{
protected:
    // The store is shared by the whole run, so every test starts with no value of its own.
    void SetUp() override
    {
        QSettings(QSettings::IniFormat, QSettings::UserScope, "aspia", "client").clear();
    }
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, SessionTypeIsDesktopUntilChosen)
{
    EXPECT_EQ(UserSettings().sessionType(), proto::peer::SESSION_TYPE_DESKTOP);

    UserSettings().setSessionType(proto::peer::SESSION_TYPE_TERMINAL);
    EXPECT_EQ(UserSettings().sessionType(), proto::peer::SESSION_TYPE_TERMINAL);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, OneTimePasswordIsNotCheckedUntilChosen)
{
    EXPECT_FALSE(UserSettings().isOneTimePasswordChecked());

    UserSettings().setOneTimePasswordChecked(true);
    EXPECT_TRUE(UserSettings().isOneTimePasswordChecked());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, WindowIsPlacedByTheSystemUntilSaved)
{
    EXPECT_TRUE(UserSettings().windowGeometry().isEmpty());
    EXPECT_TRUE(UserSettings().windowState().isEmpty());

    UserSettings().setWindowGeometry("geometry");
    UserSettings().setWindowState("state");

    EXPECT_EQ(UserSettings().windowGeometry(), QByteArray("geometry"));
    EXPECT_EQ(UserSettings().windowState(), QByteArray("state"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, DialogsArePlacedByTheSystemUntilSaved)
{
    EXPECT_TRUE(UserSettings().quickConnectState().isEmpty());
    EXPECT_TRUE(UserSettings().dialogGeometry("first").isEmpty());

    UserSettings().setQuickConnectState("quick");
    UserSettings().setDialogGeometry("first", "geometry");

    EXPECT_EQ(UserSettings().quickConnectState(), QByteArray("quick"));
    EXPECT_EQ(UserSettings().dialogGeometry("first"), QByteArray("geometry"));
    EXPECT_TRUE(UserSettings().dialogGeometry("second").isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, ToolBarIsShownWithSmallIconsUntilChosen)
{
    EXPECT_FALSE(UserSettings().largeIcons());
    EXPECT_TRUE(UserSettings().isToolBarEnabled());
    EXPECT_TRUE(UserSettings().isSearchFieldEnabled());

    UserSettings().setLargeIcons(true);
    UserSettings().setToolBarEnabled(false);
    UserSettings().setSearchFieldEnabled(false);

    EXPECT_TRUE(UserSettings().largeIcons());
    EXPECT_FALSE(UserSettings().isToolBarEnabled());
    EXPECT_FALSE(UserSettings().isSearchFieldEnabled());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, SessionsOpenInTabsBelowOtherWindowsUntilChosen)
{
    EXPECT_FALSE(UserSettings().alwaysOnTop());
    EXPECT_TRUE(UserSettings().openSessionsInTabs());

    UserSettings().setAlwaysOnTop(true);
    UserSettings().setOpenSessionsInTabs(false);

    EXPECT_TRUE(UserSettings().alwaysOnTop());
    EXPECT_FALSE(UserSettings().openSessionsInTabs());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, OnlineIsCheckedWithNoDirectoryUntilChosen)
{
    EXPECT_TRUE(UserSettings().isOnlineCheckEnabled());
    EXPECT_TRUE(UserSettings().lastDirectory().isEmpty());

    UserSettings().setOnlineCheckEnabled(false);
    UserSettings().setLastDirectory("C:/files");

    EXPECT_FALSE(UserSettings().isOnlineCheckEnabled());
    EXPECT_EQ(UserSettings().lastDirectory(), QString("C:/files"));
}

//--------------------------------------------------------------------------------------------------
// The application closes without a question until the user turns it on.
TEST_F(UserSettingsTest, CloseConfirmationIsOffUntilChosen)
{
    EXPECT_FALSE(UserSettings().isConfirmCloseEnabled());

    UserSettings().setConfirmCloseEnabled(true);
    EXPECT_TRUE(UserSettings().isConfirmCloseEnabled());

    UserSettings().setConfirmCloseEnabled(false);
    EXPECT_FALSE(UserSettings().isConfirmCloseEnabled());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, UpdatesAreCheckedUntilChosen)
{
    EXPECT_TRUE(UserSettings().isCheckUpdatesEnabled());

    UserSettings().setCheckUpdatesEnabled(false);
    EXPECT_FALSE(UserSettings().isCheckUpdatesEnabled());

    UserSettings().setCheckUpdatesEnabled(true);
    EXPECT_TRUE(UserSettings().isCheckUpdatesEnabled());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserSettingsTest, UpdateChannelIsStableUntilChosen)
{
    EXPECT_EQ(UserSettings().updateChannel(), QString(kStableUpdateChannel));

    UserSettings().setUpdateChannel(kBetaUpdateChannel);
    EXPECT_EQ(UserSettings().updateChannel(), QString(kBetaUpdateChannel));
}

//--------------------------------------------------------------------------------------------------
// The MCP connector is not available until the user allows it.
TEST_F(UserSettingsTest, McpIsOffUntilChosen)
{
    EXPECT_FALSE(UserSettings().isMcpEnabled());

    UserSettings().setMcpEnabled(true);
    EXPECT_TRUE(UserSettings().isMcpEnabled());

    UserSettings().setMcpEnabled(false);
    EXPECT_FALSE(UserSettings().isMcpEnabled());
}

//--------------------------------------------------------------------------------------------------
// The user confirms the actions of the MCP connector until automatic confirmation is turned on.
TEST_F(UserSettingsTest, McpAutoConfirmIsOffUntilChosen)
{
    EXPECT_FALSE(UserSettings().isMcpAutoConfirmEnabled());

    UserSettings().setMcpAutoConfirmEnabled(true);
    EXPECT_TRUE(UserSettings().isMcpAutoConfirmEnabled());

    UserSettings().setMcpAutoConfirmEnabled(false);
    EXPECT_FALSE(UserSettings().isMcpAutoConfirmEnabled());
}

//--------------------------------------------------------------------------------------------------
// MCP signs in with the saved credentials until the user turns it off.
TEST_F(UserSettingsTest, McpSignInIsOnUntilChosen)
{
    EXPECT_TRUE(UserSettings().isMcpSignInEnabled());

    UserSettings().setMcpSignInEnabled(false);
    EXPECT_FALSE(UserSettings().isMcpSignInEnabled());

    UserSettings().setMcpSignInEnabled(true);
    EXPECT_TRUE(UserSettings().isMcpSignInEnabled());
}

//--------------------------------------------------------------------------------------------------
// MCP works with every type of session until the user chooses the types. No type is a choice too.
TEST_F(UserSettingsTest, McpSessionTypesAreAllUntilChosen)
{
    EXPECT_EQ(UserSettings().mcpSessionTypes(), static_cast<quint32>(proto::peer::SESSION_TYPE_ALL));

    UserSettings().setMcpSessionTypes(proto::peer::SESSION_TYPE_DESKTOP | proto::peer::SESSION_TYPE_TERMINAL);
    EXPECT_EQ(UserSettings().mcpSessionTypes(),
              static_cast<quint32>(proto::peer::SESSION_TYPE_DESKTOP | proto::peer::SESSION_TYPE_TERMINAL));

    UserSettings().setMcpSessionTypes(0);
    EXPECT_EQ(UserSettings().mcpSessionTypes(), 0u);
}
