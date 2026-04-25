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

#include "base/files/base_paths.h"

#include <QDir>
#include <QFileInfo>

#include <gtest/gtest.h>

namespace base {

namespace {

// Verifies the common invariants for any directory path returned by BasePaths:
// non-empty, absolute, no native (back)slashes left over.
void expectValidDir(const QString& path)
{
    EXPECT_FALSE(path.isEmpty());
    EXPECT_TRUE(QDir::isAbsolutePath(path));
    EXPECT_FALSE(path.contains('\\'));
}

} // namespace

// ============================================================================
// Generic config / data directories
// ============================================================================

TEST(BasePathsTest, GenericConfigDir)
{
    expectValidDir(BasePaths::genericConfigDir());
    EXPECT_EQ(BasePaths::genericConfigDir(), BasePaths::genericConfigDir());
}

TEST(BasePathsTest, GenericUserConfigDir)
{
    expectValidDir(BasePaths::genericUserConfigDir());
    EXPECT_EQ(BasePaths::genericUserConfigDir(), BasePaths::genericUserConfigDir());
}

TEST(BasePathsTest, GenericDataDir)
{
    expectValidDir(BasePaths::genericDataDir());
    EXPECT_EQ(BasePaths::genericDataDir(), BasePaths::genericDataDir());
}

TEST(BasePathsTest, GenericUserDataDir)
{
    expectValidDir(BasePaths::genericUserDataDir());
    EXPECT_EQ(BasePaths::genericUserDataDir(), BasePaths::genericUserDataDir());
}

// ============================================================================
// Application-level directories: must be the corresponding generic path
// with "/aspia" appended.
// ============================================================================

TEST(BasePathsTest, AppConfigDir)
{
    expectValidDir(BasePaths::appConfigDir());
    EXPECT_EQ(BasePaths::appConfigDir(), BasePaths::genericConfigDir() + "/aspia");
}

TEST(BasePathsTest, AppUserConfigDir)
{
    expectValidDir(BasePaths::appUserConfigDir());
    EXPECT_EQ(BasePaths::appUserConfigDir(), BasePaths::genericUserConfigDir() + "/aspia");
}

TEST(BasePathsTest, AppDataDir)
{
    expectValidDir(BasePaths::appDataDir());
    EXPECT_EQ(BasePaths::appDataDir(), BasePaths::genericDataDir() + "/aspia");
}

TEST(BasePathsTest, AppUserDataDir)
{
    expectValidDir(BasePaths::appUserDataDir());
    EXPECT_EQ(BasePaths::appUserDataDir(), BasePaths::genericUserDataDir() + "/aspia");
}

// ============================================================================
// User home
// ============================================================================

TEST(BasePathsTest, UserHome)
{
    QString home = BasePaths::userHome();
    expectValidDir(home);
    EXPECT_TRUE(QDir(home).exists());
}

// ============================================================================
// Current executable
// ============================================================================

TEST(BasePathsTest, CurrentApp)
{
    QString path = BasePaths::currentApp();
    EXPECT_FALSE(path.isEmpty());

    QFileInfo info(path);
    EXPECT_TRUE(info.exists());
    EXPECT_TRUE(info.isFile());
    EXPECT_TRUE(info.isAbsolute());
}

TEST(BasePathsTest, CurrentAppDir)
{
    QString dir = BasePaths::currentAppDir();
    expectValidDir(dir);
    EXPECT_TRUE(QDir(dir).exists());
    EXPECT_EQ(dir, QFileInfo(BasePaths::currentApp()).absolutePath());
}

// ============================================================================
// On POSIX systems, all per-user directories must live under the user's home.
// ============================================================================

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

TEST(BasePathsTest, PosixUserDirsLiveUnderHome)
{
    QString home = BasePaths::userHome();
    ASSERT_FALSE(home.isEmpty());

    EXPECT_TRUE(BasePaths::genericUserConfigDir().startsWith(home));
    EXPECT_TRUE(BasePaths::genericUserDataDir().startsWith(home));
    EXPECT_TRUE(BasePaths::appUserConfigDir().startsWith(home));
    EXPECT_TRUE(BasePaths::appUserDataDir().startsWith(home));
}

TEST(BasePathsTest, PosixUserHomeMatchesHomeEnv)
{
    QString env_home = qEnvironmentVariable("HOME");
    ASSERT_FALSE(env_home.isEmpty());
    EXPECT_EQ(BasePaths::userHome(), env_home);
}

#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

// ============================================================================
// Linux: exact default values (independent from XDG variables)
// ============================================================================

#if defined(Q_OS_LINUX)

TEST(BasePathsTest, LinuxGenericConfigDir)
{
    EXPECT_EQ(BasePaths::genericConfigDir(), "/etc");
}

TEST(BasePathsTest, LinuxGenericDataDir)
{
    EXPECT_EQ(BasePaths::genericDataDir(), "/var/lib");
}

TEST(BasePathsTest, LinuxAppConfigDir)
{
    EXPECT_EQ(BasePaths::appConfigDir(), "/etc/aspia");
}

TEST(BasePathsTest, LinuxAppDataDir)
{
    EXPECT_EQ(BasePaths::appDataDir(), "/var/lib/aspia");
}

// ----------------------------------------------------------------------------
// Linux: XDG_CONFIG_HOME / XDG_DATA_HOME override and fallback behavior.
// The fixture saves and restores both variables around each test so we can
// freely set/unset them without affecting other tests.
// ----------------------------------------------------------------------------

class BasePathsLinuxXdgTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        had_config_home_ = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
        if (had_config_home_)
            saved_config_home_ = qgetenv("XDG_CONFIG_HOME");

        had_data_home_ = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        if (had_data_home_)
            saved_data_home_ = qgetenv("XDG_DATA_HOME");
    }

    void TearDown() override
    {
        if (had_config_home_)
            qputenv("XDG_CONFIG_HOME", saved_config_home_);
        else
            qunsetenv("XDG_CONFIG_HOME");

        if (had_data_home_)
            qputenv("XDG_DATA_HOME", saved_data_home_);
        else
            qunsetenv("XDG_DATA_HOME");
    }

private:
    QByteArray saved_config_home_;
    QByteArray saved_data_home_;
    bool had_config_home_ = false;
    bool had_data_home_ = false;
};

TEST_F(BasePathsLinuxXdgTest, GenericUserConfigDirUsesXdgConfigHome)
{
    ASSERT_TRUE(qputenv("XDG_CONFIG_HOME", "/tmp/aspia_test_config"));
    EXPECT_EQ(BasePaths::genericUserConfigDir(), "/tmp/aspia_test_config");
}

TEST_F(BasePathsLinuxXdgTest, GenericUserConfigDirFallsBackToHomeConfigWhenUnset)
{
    qunsetenv("XDG_CONFIG_HOME");
    EXPECT_EQ(BasePaths::genericUserConfigDir(), BasePaths::userHome() + "/.config");
}

TEST_F(BasePathsLinuxXdgTest, GenericUserConfigDirFallsBackToHomeConfigWhenEmpty)
{
    ASSERT_TRUE(qputenv("XDG_CONFIG_HOME", ""));
    EXPECT_EQ(BasePaths::genericUserConfigDir(), BasePaths::userHome() + "/.config");
}

TEST_F(BasePathsLinuxXdgTest, GenericUserDataDirUsesXdgDataHome)
{
    ASSERT_TRUE(qputenv("XDG_DATA_HOME", "/tmp/aspia_test_data"));
    EXPECT_EQ(BasePaths::genericUserDataDir(), "/tmp/aspia_test_data");
}

TEST_F(BasePathsLinuxXdgTest, GenericUserDataDirFallsBackToHomeShareWhenUnset)
{
    qunsetenv("XDG_DATA_HOME");
    EXPECT_EQ(BasePaths::genericUserDataDir(), BasePaths::userHome() + "/.local/share");
}

TEST_F(BasePathsLinuxXdgTest, GenericUserDataDirFallsBackToHomeShareWhenEmpty)
{
    ASSERT_TRUE(qputenv("XDG_DATA_HOME", ""));
    EXPECT_EQ(BasePaths::genericUserDataDir(), BasePaths::userHome() + "/.local/share");
}

#endif // defined(Q_OS_LINUX)

// ============================================================================
// macOS: exact default values (relative to user home)
// ============================================================================

#if defined(Q_OS_MACOS)

TEST(BasePathsTest, MacOSGenericConfigDir)
{
    EXPECT_EQ(BasePaths::genericConfigDir(), "/Library/Application Support");
}

TEST(BasePathsTest, MacOSGenericDataDir)
{
    EXPECT_EQ(BasePaths::genericDataDir(), "/Library/Application Support");
}

TEST(BasePathsTest, MacOSAppConfigDir)
{
    EXPECT_EQ(BasePaths::appConfigDir(), "/Library/Application Support/aspia");
}

TEST(BasePathsTest, MacOSAppDataDir)
{
    EXPECT_EQ(BasePaths::appDataDir(), "/Library/Application Support/aspia");
}

TEST(BasePathsTest, MacOSGenericUserConfigDir)
{
    EXPECT_EQ(BasePaths::genericUserConfigDir(),
              BasePaths::userHome() + "/Library/Application Support");
}

TEST(BasePathsTest, MacOSGenericUserDataDir)
{
    EXPECT_EQ(BasePaths::genericUserDataDir(),
              BasePaths::userHome() + "/Library/Application Support");
}

TEST(BasePathsTest, MacOSAppUserConfigDir)
{
    EXPECT_EQ(BasePaths::appUserConfigDir(),
              BasePaths::userHome() + "/Library/Application Support/aspia");
}

TEST(BasePathsTest, MacOSAppUserDataDir)
{
    EXPECT_EQ(BasePaths::appUserDataDir(),
              BasePaths::userHome() + "/Library/Application Support/aspia");
}

#endif // defined(Q_OS_MACOS)

// ============================================================================
// Windows: validate against the standard environment variables. SHGetFolderPathW
// uses the same well-known folders, so the values must agree.
// ============================================================================

#if defined(Q_OS_WIN)

TEST(BasePathsTest, WindowsUserHomeMatchesUserprofile)
{
    QString env = qEnvironmentVariable("USERPROFILE");
    ASSERT_FALSE(env.isEmpty());
    EXPECT_EQ(BasePaths::userHome(), QDir::fromNativeSeparators(env));
}

TEST(BasePathsTest, WindowsGenericConfigDirMatchesProgramData)
{
    QString env = qEnvironmentVariable("PROGRAMDATA");
    ASSERT_FALSE(env.isEmpty());
    EXPECT_EQ(BasePaths::genericConfigDir(), QDir::fromNativeSeparators(env));
}

TEST(BasePathsTest, WindowsGenericUserConfigDirMatchesAppData)
{
    QString env = qEnvironmentVariable("APPDATA");
    ASSERT_FALSE(env.isEmpty());
    EXPECT_EQ(BasePaths::genericUserConfigDir(), QDir::fromNativeSeparators(env));
}

TEST(BasePathsTest, WindowsGenericDataDirEqualsConfigDir)
{
    EXPECT_EQ(BasePaths::genericDataDir(), BasePaths::genericConfigDir());
}

TEST(BasePathsTest, WindowsGenericUserDataDirEqualsUserConfigDir)
{
    EXPECT_EQ(BasePaths::genericUserDataDir(), BasePaths::genericUserConfigDir());
}

TEST(BasePathsTest, WindowsAppConfigDir)
{
    EXPECT_EQ(BasePaths::appConfigDir(), BasePaths::genericConfigDir() + "/aspia");
}

TEST(BasePathsTest, WindowsAppUserConfigDir)
{
    EXPECT_EQ(BasePaths::appUserConfigDir(), BasePaths::genericUserConfigDir() + "/aspia");
}

#endif // defined(Q_OS_WIN)

} // namespace base
