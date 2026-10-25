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

#include "router/settings.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

class RouterSettingsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/router.conf";
        qputenv("ASPIA_ROUTER_CONFIG_FILE", file_path_.toLocal8Bit());
    }

    void TearDown() override
    {
        qunsetenv("ASPIA_ROUTER_CONFIG_FILE");
    }

    void writeConfig(const QByteArray& content)
    {
        QFile file(file_path_);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        ASSERT_EQ(file.write(content), content.size());
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
};

//--------------------------------------------------------------------------------------------------
// A configuration written before the access log existed keeps the entries for the default period.
TEST_F(RouterSettingsTest, AccessLogDaysDefaultToNinety)
{
    EXPECT_EQ(Settings().accessLogDays(), 90);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterSettingsTest, AccessLogDaysReadBack)
{
    {
        Settings settings;
        settings.setAccessLogDays(30);
        ASSERT_TRUE(settings.sync());
    }

    EXPECT_EQ(Settings().accessLogDays(), 30);
}

//--------------------------------------------------------------------------------------------------
// --create-config writes the key with its default, so an administrator finds it in the file.
TEST_F(RouterSettingsTest, ResetWritesTheDefaultAccessLogDays)
{
    {
        Settings settings;
        settings.setAccessLogDays(30);
        settings.reset();
        ASSERT_TRUE(settings.sync());
    }

    EXPECT_EQ(Settings().accessLogDays(), 90);

    QFile file(file_path_);
    ASSERT_TRUE(file.open(QFile::ReadOnly));
    EXPECT_TRUE(file.readAll().contains("access_log_days"));
}

//--------------------------------------------------------------------------------------------------
// A wrong number in the file does not stop the router. It is taken as the nearest bound of the range.
TEST_F(RouterSettingsTest, AccessLogDaysOutOfRangeAreClamped)
{
    const struct { const char* value; quint16 days; } kCases[] = {
        { "0", 1 },
        { "-5", 1 },
        { "1", 1 },
        { "365", 365 },
        { "366", 365 },
        { "100000", 365 }
    };

    for (const auto& test_case : kCases)
    {
        writeConfig(QByteArray("[router]\naccess_log_days=") + test_case.value + "\n");
        EXPECT_EQ(Settings().accessLogDays(), test_case.days) << test_case.value;
    }
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterSettingsTest, AccessLogDaysThatAreNotANumberAreTheDefault)
{
    writeConfig("[router]\naccess_log_days=abc\n");
    EXPECT_EQ(Settings().accessLogDays(), 90);
}
