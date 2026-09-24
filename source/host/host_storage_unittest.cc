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

#include "host/host_storage.h"

#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <ctime>

namespace {

const qint64 kPeriod = 7 * 24 * 60 * 60;
const int kMaxServiceStarts = 1000;

} // namespace

class HostStorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());

        // The storage lives in the scope of the machine (of the user on Android). Moving both scopes
        // into the temporary directory keeps the test off the real storage.
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temp_dir_.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    }

    // Writes a list of timepoints the way the storage keeps them.
    static void setTimepoints(const QString& key, const QList<qint64>& timepoints)
    {
        QStringList list;
        for (qint64 timepoint : timepoints)
            list.append(QString::number(timepoint));

        QSettings(QSettings::IniFormat, QSettings::SystemScope, "aspia", "host_storage")
            .setValue(key, list);
    }

    static void setServiceStarts(const QList<qint64>& starts)
    {
        setTimepoints("service_starts", starts);
    }

    QTemporaryDir temp_dir_;
};

//--------------------------------------------------------------------------------------------------
// Only the starts of the last 7 days are counted, and a new start is added to them.
TEST_F(HostStorageTest, ServiceStartsOfTheLastWeekAreCounted)
{
    const qint64 current_time = std::time(nullptr);
    setServiceStarts({ current_time - kPeriod - 60, current_time - 60 });

    HostStorage storage;
    EXPECT_EQ(storage.serviceStartTime(), current_time - 60);
    EXPECT_EQ(storage.serviceStartCount(), 1);

    storage.registerServiceStart();

    EXPECT_GE(storage.serviceStartTime(), current_time);
    EXPECT_EQ(storage.serviceStartCount(), 2);
}

//--------------------------------------------------------------------------------------------------
// Successful logins are counted over the last 7 days and since the last start of the service.
TEST_F(HostStorageTest, SuccessfulLoginsAreCounted)
{
    const qint64 current_time = std::time(nullptr);
    setServiceStarts({ current_time - 100 });
    setTimepoints("successful_logins", { current_time - kPeriod - 60, current_time - 200, current_time - 50 });

    HostStorage storage;
    EXPECT_EQ(storage.successfulLoginCount(), 2);
    EXPECT_EQ(storage.successfulLoginCountSinceStart(), 1);

    storage.registerSuccessfulLogin();

    EXPECT_EQ(storage.successfulLoginCount(), 3);
    EXPECT_EQ(storage.successfulLoginCountSinceStart(), 2);
}

//--------------------------------------------------------------------------------------------------
// Failed logins are counted over the last 7 days and since the last start of the service.
TEST_F(HostStorageTest, FailedLoginsAreCounted)
{
    const qint64 current_time = std::time(nullptr);
    setServiceStarts({ current_time - 100 });
    setTimepoints("failed_logins", { current_time - kPeriod - 60, current_time - 200, current_time - 50 });

    HostStorage storage;
    EXPECT_EQ(storage.failedLoginCount(), 2);
    EXPECT_EQ(storage.failedLoginCountSinceStart(), 1);

    storage.registerFailedLogin();

    EXPECT_EQ(storage.failedLoginCount(), 3);
    EXPECT_EQ(storage.failedLoginCountSinceStart(), 2);
}

//--------------------------------------------------------------------------------------------------
// Connections to the router are counted over the last 7 days and since the last start of the
// service.
TEST_F(HostStorageTest, RouterConnectsAreCounted)
{
    const qint64 current_time = std::time(nullptr);
    setServiceStarts({ current_time - 100 });
    setTimepoints("router_connects", { current_time - kPeriod - 60, current_time - 200, current_time - 50 });

    HostStorage storage;
    EXPECT_EQ(storage.routerConnectCount(), 2);
    EXPECT_EQ(storage.routerConnectCountSinceStart(), 1);

    storage.registerRouterConnect();

    EXPECT_EQ(storage.routerConnectCount(), 3);
    EXPECT_EQ(storage.routerConnectCountSinceStart(), 2);
}

//--------------------------------------------------------------------------------------------------
// No more starts than the limit are kept. A new start still becomes the last one, the oldest start
// gives way to it.
TEST_F(HostStorageTest, ServiceStartsAreLimited)
{
    const qint64 current_time = std::time(nullptr);

    QList<qint64> starts;
    for (int i = kMaxServiceStarts; i > 0; --i)
        starts.append(current_time - i);
    setServiceStarts(starts);

    HostStorage storage;
    EXPECT_EQ(storage.serviceStartCount(), kMaxServiceStarts);

    storage.registerServiceStart();

    EXPECT_EQ(storage.serviceStartCount(), kMaxServiceStarts);
    EXPECT_GE(storage.serviceStartTime(), current_time);
}
