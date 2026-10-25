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

#include "router/access_log.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

//--------------------------------------------------------------------------------------------------
AccessLog::Record makeRecord(std::string_view user_name, HostId host_id)
{
    AccessLog::Record record;
    record.user_id = 1;
    record.user_name = user_name;
    record.session_type = proto::peer::SESSION_TYPE_DESKTOP;
    record.client_address = "10.0.0.1";
    record.client_computer_name = "client-pc";
    record.client_os_name = "Windows 11";
    record.client_version = "3.2.0";
    record.host_id = host_id;
    record.host_computer_name = "host-pc";
    record.host_address = "10.0.0.2";
    return record;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> entryIds(const proto::router::AccessLogList& list)
{
    QList<qint64> ids;
    for (const proto::router::AccessLogEntry& entry : list.entry())
        ids.append(entry.entry_id());
    return ids;
}

} // namespace

class AccessLogTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/access_log.db3";
        ASSERT_TRUE(log_.open(file_path_));
    }

    proto::router::AccessLogList entries(const proto::router::AccessLogFilter& filter, qint64 max_entry_id = 0,
        qint64 offset = 0, qint64 count = proto::router::kMaxAccessLogPageSize)
    {
        proto::router::AccessLogList list;
        log_.entries(filter, max_entry_id, offset, count, &list);
        return list;
    }

    bool execRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;
        return raw.exec(sql.toStdString().c_str());
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
    AccessLog log_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogTest, EntryReadsBack)
{
    const qint64 before = secondsSinceEpoch();
    ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(7))));

    const proto::router::AccessLogList list = entries(proto::router::AccessLogFilter());
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.entry_size(), 1);
    EXPECT_EQ(list.total_count(), 1);
    EXPECT_EQ(list.max_entry_id(), list.entry(0).entry_id());

    const proto::router::AccessLogEntry& entry = list.entry(0);
    EXPECT_GE(entry.time(), before);
    EXPECT_LE(entry.time(), secondsSinceEpoch());
    EXPECT_EQ(entry.user_id(), 1);
    EXPECT_EQ(entry.user_name(), "admin");
    EXPECT_EQ(entry.session_type(), proto::peer::SESSION_TYPE_DESKTOP);
    EXPECT_EQ(entry.client_address(), "10.0.0.1");
    EXPECT_EQ(entry.client_computer_name(), "client-pc");
    EXPECT_EQ(entry.client_os_name(), "Windows 11");
    EXPECT_EQ(entry.client_version(), "3.2.0");
    EXPECT_EQ(entry.host_id(), HostId(7));
    EXPECT_EQ(entry.host_computer_name(), "host-pc");
    EXPECT_EQ(entry.host_address(), "10.0.0.2");
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogTest, EmptyLogIsPinnedToNothing)
{
    const proto::router::AccessLogList list = entries(proto::router::AccessLogFilter());
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.entry_size(), 0);
    EXPECT_EQ(list.total_count(), 0);
    EXPECT_EQ(list.max_entry_id(), 0);
}

//--------------------------------------------------------------------------------------------------
// The newest entries come first, and the entries written after the first page do not shift the
// pages that follow it.
TEST_F(AccessLogTest, PagesArePinnedToTheFirstOne)
{
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));

    const proto::router::AccessLogList first = entries(proto::router::AccessLogFilter(), 0, 0, 2);
    ASSERT_EQ(first.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(entryIds(first), QList<qint64>({ 5, 4 }));
    EXPECT_EQ(first.total_count(), 5);
    EXPECT_EQ(first.max_entry_id(), 5);

    for (int i = 0; i < 2; ++i)
        ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));

    QList<qint64> glued = entryIds(first);
    for (qint64 offset = 2; offset < 5; offset += 2)
    {
        const proto::router::AccessLogList page = entries(proto::router::AccessLogFilter(), 5, offset, 2);
        ASSERT_EQ(page.error_code(), proto::router::kErrorOk);
        EXPECT_EQ(page.total_count(), 5);
        EXPECT_EQ(page.max_entry_id(), 5);
        glued.append(entryIds(page));
    }
    EXPECT_EQ(glued, QList<qint64>({ 5, 4, 3, 2, 1 }));

    const proto::router::AccessLogList reloaded = entries(proto::router::AccessLogFilter(), 0, 0, 2);
    EXPECT_EQ(entryIds(reloaded), QList<qint64>({ 7, 6 }));
    EXPECT_EQ(reloaded.total_count(), 7);
    EXPECT_EQ(reloaded.max_entry_id(), 7);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogTest, FilterNarrowsTheEntries)
{
    AccessLog::Record temp_host_record = makeRecord("admin", kMinTempHostId + 12345);
    temp_host_record.session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
    temp_host_record.host_computer_name = "Laptop-7";
    temp_host_record.host_address = "172.16.5.5";
    ASSERT_TRUE(log_.addEntry(temp_host_record));

    AccessLog::Record boris_record = makeRecord("Борис", HostId(1));
    boris_record.client_address = "192.168.1.50";
    boris_record.client_computer_name = "Workstation";
    boris_record.host_computer_name = "Бухгалтерия";
    ASSERT_TRUE(log_.addEntry(boris_record));

    AccessLog::Record admin_record = makeRecord("admin", HostId(1));
    admin_record.host_computer_name = "Бухгалтерия";
    ASSERT_TRUE(log_.addEntry(admin_record));

    ASSERT_TRUE(execRaw("UPDATE access_log SET time=1000 WHERE id=1"));
    ASSERT_TRUE(execRaw("UPDATE access_log SET time=2000 WHERE id=2"));
    ASSERT_TRUE(execRaw("UPDATE access_log SET time=3000 WHERE id=3"));

    auto matches = [this](const proto::router::AccessLogFilter& filter)
    {
        const proto::router::AccessLogList list = entries(filter);
        EXPECT_EQ(list.error_code(), proto::router::kErrorOk);
        EXPECT_EQ(list.total_count(), list.entry_size());
        return entryIds(list);
    };

    proto::router::AccessLogFilter filter;
    filter.set_user_name("бОр");
    EXPECT_EQ(matches(filter), QList<qint64>({ 2 }));

    filter.Clear();
    filter.set_client("168.1");
    EXPECT_EQ(matches(filter), QList<qint64>({ 2 }));

    filter.Clear();
    filter.set_client("WORKST");
    EXPECT_EQ(matches(filter), QList<qint64>({ 2 }));

    filter.Clear();
    filter.set_client("10_0");
    EXPECT_TRUE(matches(filter).isEmpty());

    filter.Clear();
    filter.set_host("бухг");
    EXPECT_EQ(matches(filter), QList<qint64>({ 3, 2 }));

    filter.Clear();
    filter.set_host("laptop");
    EXPECT_EQ(matches(filter), QList<qint64>({ 1 }));

    filter.Clear();
    filter.set_host("172.16");
    EXPECT_EQ(matches(filter), QList<qint64>({ 1 }));

    filter.Clear();
    filter.set_host("12345");
    EXPECT_EQ(matches(filter), QList<qint64>({ 1 }));

    filter.Clear();
    filter.set_session_type(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    EXPECT_EQ(matches(filter), QList<qint64>({ 1 }));

    filter.Clear();
    filter.set_time_from(2000);
    EXPECT_EQ(matches(filter), QList<qint64>({ 3, 2 }));

    filter.Clear();
    filter.set_time_to(2000);
    EXPECT_EQ(matches(filter), QList<qint64>({ 1 }));

    filter.Clear();
    filter.set_time_from(1500);
    filter.set_time_to(3000);
    EXPECT_EQ(matches(filter), QList<qint64>({ 2 }));

    filter.Clear();
    filter.set_user_name("admin");
    filter.set_host("Бухгалтерия");
    EXPECT_EQ(matches(filter), QList<qint64>({ 3 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogTest, RefusesAnInvalidRequest)
{
    ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));

    const proto::router::AccessLogFilter no_filter;
    EXPECT_EQ(entries(no_filter, 0, 0, 0).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(entries(no_filter, 0, 0, proto::router::kMaxAccessLogPageSize + 1).error_code(),
              proto::router::kErrorInvalidRequest);
    EXPECT_EQ(entries(no_filter, 0, -1).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(entries(no_filter, -1).error_code(), proto::router::kErrorInvalidRequest);

    proto::router::AccessLogFilter filter;
    filter.set_time_from(-1);
    EXPECT_EQ(entries(filter).error_code(), proto::router::kErrorInvalidRequest);

    filter.Clear();
    filter.set_time_to(-1);
    EXPECT_EQ(entries(filter).error_code(), proto::router::kErrorInvalidRequest);

    const std::string too_long(proto::router::kMaxAccessLogQueryLength + 1, 'a');

    filter.Clear();
    filter.set_user_name(too_long);
    EXPECT_EQ(entries(filter).error_code(), proto::router::kErrorInvalidRequest);

    filter.Clear();
    filter.set_client(too_long);
    EXPECT_EQ(entries(filter).error_code(), proto::router::kErrorInvalidRequest);

    filter.Clear();
    filter.set_host(too_long);
    const proto::router::AccessLogList refused = entries(filter);
    EXPECT_EQ(refused.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(refused.entry_size(), 0);
    EXPECT_EQ(refused.total_count(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogTest, PruneDropsOnlyExpiredEntries)
{
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));

    const qint64 day = 24 * 3600;
    const qint64 now = secondsSinceEpoch();
    ASSERT_TRUE(execRaw(QString("UPDATE access_log SET time=%1 WHERE id=1").arg(now - 91 * day)));
    ASSERT_TRUE(execRaw(QString("UPDATE access_log SET time=%1 WHERE id=2").arg(now - 89 * day)));

    EXPECT_FALSE(log_.prune(0));
    EXPECT_FALSE(log_.prune(-1));
    EXPECT_EQ(entries(proto::router::AccessLogFilter()).total_count(), 3);

    ASSERT_TRUE(log_.prune(90));
    EXPECT_EQ(entryIds(entries(proto::router::AccessLogFilter())), QList<qint64>({ 3, 2 }));

    ASSERT_TRUE(log_.prune(1));
    EXPECT_EQ(entryIds(entries(proto::router::AccessLogFilter())), QList<qint64>({ 3 }));
}

//--------------------------------------------------------------------------------------------------
// Pruning every row must not let the ids start over, or a page pinned before it would take in the
// entries written after it.
TEST_F(AccessLogTest, IdsKeepGrowingAfterPrune)
{
    ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));
    ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));
    ASSERT_TRUE(execRaw("UPDATE access_log SET time=0"));
    ASSERT_TRUE(log_.prune(1));

    ASSERT_TRUE(log_.addEntry(makeRecord("admin", HostId(1))));
    EXPECT_EQ(entryIds(entries(proto::router::AccessLogFilter())), QList<qint64>({ 3 }));
}
