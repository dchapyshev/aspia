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

#include "client/desktop/management/access_log_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "base/peer/host_id.h"
#include "proto/peer.h"

namespace {

using Column = AccessLogListModel::Column;

// Where the tester of Qt reports what it found. It has no way to fail a test of gtest by itself,
// so its complaints are picked out of the messages of Qt.
QStringList* g_contract_failures = nullptr;
QtMessageHandler g_previous_handler = nullptr;

//--------------------------------------------------------------------------------------------------
void recordingMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (type == QtWarningMsg && g_contract_failures)
        g_contract_failures->append(msg);

    if (g_previous_handler)
        g_previous_handler(type, context, msg);
}

//--------------------------------------------------------------------------------------------------
void addEntry(proto::router::AccessLogList* list, qint64 entry_id, qint64 time, HostId host_id,
              const QString& user_name)
{
    proto::router::AccessLogEntry* entry = list->add_entry();
    entry->set_entry_id(entry_id);
    entry->set_time(time);
    entry->set_user_id(1);
    entry->set_user_name(user_name.toStdString());
    entry->set_session_type(proto::peer::SESSION_TYPE_DESKTOP);
    entry->set_client_address("192.168.0.1");
    entry->set_client_computer_name("client-pc");
    entry->set_client_version("3.2.0");
    entry->set_client_os_name("Windows 11");
    entry->set_host_id(host_id);
    entry->set_host_computer_name("host-pc");
    entry->set_host_address("10.0.0.2");
}

} // namespace

// Every model of the fixture is watched by the tester of Qt. It checks that the counts agree with
// the indexes handed out and that the signals around a change come in the order and the pairing
// the views rely on.
class AccessLogListModelTest : public testing::Test
{
protected:
    void SetUp() override
    {
        g_contract_failures = &contract_failures_;
        g_previous_handler = qInstallMessageHandler(recordingMessageHandler);
    }

    void TearDown() override
    {
        tester_.reset();
        model_.reset();

        qInstallMessageHandler(g_previous_handler);
        g_previous_handler = nullptr;
        g_contract_failures = nullptr;

        EXPECT_TRUE(contract_failures_.isEmpty())
            << contract_failures_.join('\n').toStdString();
    }

    AccessLogListModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<AccessLogListModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // Three entries whose times, host ids and user names each sort in a different order, and whose
    // host ids and names only sort right when read as numbers.
    static proto::router::AccessLogList entries()
    {
        proto::router::AccessLogList list;

        addEntry(&list, 3, 3000, HostId(9), "user2");
        addEntry(&list, 2, 1000, HostId(100), "user10");
        addEntry(&list, 1, 2000, HostId(10), "alpha");

        return list;
    }

    static QList<qint64> rowIds(const AccessLogListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->entryAt(i)->entry_id());
        return ids;
    }

    static QVariant dataAt(const AccessLogListModel* model, int row, Column column, int role = Qt::DisplayRole)
    {
        return model->index(row, static_cast<int>(column)).data(role);
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<AccessLogListModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 10);
    EXPECT_EQ(model()->entryAt(0), nullptr);
    EXPECT_EQ(model()->rowOf(1), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Time"));
    EXPECT_EQ(model()->headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Session Type"));
    EXPECT_EQ(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Host Address"));

    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(10, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, RowShowsTheFieldsOfItsEntry)
{
    model()->setEntries(entries());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_FALSE(dataAt(model(), 0, Column::TIME).toString().isEmpty());
    EXPECT_EQ(dataAt(model(), 0, Column::USER).toString(), QString("user2"));
    EXPECT_EQ(dataAt(model(), 0, Column::SESSION_TYPE).toString(), QString("Desktop"));
    EXPECT_EQ(dataAt(model(), 0, Column::CLIENT_ADDRESS).toString(), QString("192.168.0.1"));
    EXPECT_EQ(dataAt(model(), 0, Column::CLIENT_COMPUTER_NAME).toString(), QString("client-pc"));
    EXPECT_EQ(dataAt(model(), 0, Column::CLIENT_VERSION).toString(), QString("3.2.0"));
    EXPECT_EQ(dataAt(model(), 0, Column::CLIENT_OS).toString(), QString("Windows 11"));
    EXPECT_EQ(dataAt(model(), 0, Column::HOST_ID).toString(), QString("9"));
    EXPECT_EQ(dataAt(model(), 0, Column::HOST_COMPUTER_NAME).toString(), QString("host-pc"));
    EXPECT_EQ(dataAt(model(), 0, Column::HOST_ADDRESS).toString(), QString("10.0.0.2"));
}

//--------------------------------------------------------------------------------------------------
// The type is what the client said. An old client says nothing, and a value the client does not
// know names no session either.
TEST_F(AccessLogListModelTest, SessionTypeWithoutANameIsShownAsUnknown)
{
    proto::router::AccessLogList list = entries();
    list.mutable_entry(0)->set_session_type(proto::peer::SESSION_TYPE_UNKNOWN);
    list.mutable_entry(1)->set_session_type(static_cast<proto::peer::SessionType>(1000));
    model()->setEntries(list);

    EXPECT_EQ(dataAt(model(), 0, Column::SESSION_TYPE).toString(), QString("Unknown"));
    EXPECT_EQ(dataAt(model(), 1, Column::SESSION_TYPE).toString(), QString("Unknown"));
    EXPECT_FALSE(dataAt(model(), 0, Column::SESSION_TYPE, Qt::DecorationRole).isValid());

    EXPECT_EQ(dataAt(model(), 2, Column::SESSION_TYPE).toString(), QString("Desktop"));
    EXPECT_TRUE(dataAt(model(), 2, Column::SESSION_TYPE, Qt::DecorationRole).isValid());
    EXPECT_FALSE(dataAt(model(), 2, Column::USER, Qt::DecorationRole).isValid());
}

//--------------------------------------------------------------------------------------------------
// The cell leaves the seconds out, and the tooltip of the time has them.
TEST_F(AccessLogListModelTest, TimeHasTheFullFormInItsTooltip)
{
    model()->setEntries(entries());

    EXPECT_FALSE(dataAt(model(), 0, Column::TIME, Qt::ToolTipRole).toString().isEmpty());
    EXPECT_FALSE(dataAt(model(), 0, Column::USER, Qt::ToolTipRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, TimeIsSortedAsATime)
{
    model()->setEntries(entries());

    model()->sort(static_cast<int>(Column::TIME), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 2, 1, 3 }));

    model()->sort(static_cast<int>(Column::TIME), Qt::DescendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// As text "100" would come before "9".
TEST_F(AccessLogListModelTest, HostIdIsSortedAsANumber)
{
    model()->setEntries(entries());

    model()->sort(static_cast<int>(Column::HOST_ID), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "user2" comes before "user10".
TEST_F(AccessLogListModelTest, NameIsSortedTheWayItIsRead)
{
    model()->setEntries(entries());

    model()->sort(static_cast<int>(Column::USER), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 3, 2 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, NewPageKeepsTheSortTheUserPicked)
{
    model()->setEntries(entries());
    model()->sort(static_cast<int>(Column::USER), Qt::AscendingOrder);

    model()->setEntries(entries());

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 3, 2 }));
}

//--------------------------------------------------------------------------------------------------
// Sorting moves the rows under the selection, so the model carries it over to the same entry.
TEST_F(AccessLogListModelTest, SortingCarriesTheSelectionToItsEntry)
{
    model()->setEntries(entries());
    model()->sort(static_cast<int>(Column::USER), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->entryAt(selection.currentIndex().row())->entry_id(), 1);

    model()->sort(static_cast<int>(Column::USER), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->entryAt(selection.currentIndex().row())->entry_id(), 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, EntryIsFoundByItsId)
{
    model()->setEntries(entries());

    EXPECT_EQ(model()->rowOf(2), 1);
    EXPECT_EQ(model()->entryAt(1)->entry_id(), 2);
    EXPECT_EQ(model()->rowOf(777), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AccessLogListModelTest, ClearDropsTheRows)
{
    model()->setEntries(entries());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 10);
}
