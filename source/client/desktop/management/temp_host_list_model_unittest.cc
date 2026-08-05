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

#include "client/desktop/management/temp_host_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

namespace {

using Column = TempHostListModel::Column;

// Where the tester of Qt reports what it found. It has no way to fail a test of gtest by itself,
// so its complaints are picked out of the messages of Qt.
QStringList* g_contract_failures = nullptr;
QtMessageHandler g_previous_handler = nullptr;

//--------------------------------------------------------------------------------------------------
// The logger of the application owns the message handler. This one wraps it for the length of a
// test instead of taking its place, so the log keeps working while the failures are collected.
void recordingMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (type == QtWarningMsg && g_contract_failures)
        g_contract_failures->append(msg);

    if (g_previous_handler)
        g_previous_handler(type, context, msg);
}

//--------------------------------------------------------------------------------------------------
RouterTempHost makeHost(HostId temp_id, const QString& computer_name, const QString& address)
{
    RouterTempHost host;
    host.temp_id = temp_id;
    host.computer_name = computer_name;
    host.address = address;
    host.os_name = "Windows 11";
    host.version = "2.7.0";
    return host;
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class TempHostListModelTest : public testing::Test
{
protected:
    void SetUp() override
    {
        g_contract_failures = &contract_failures_;
        g_previous_handler = qInstallMessageHandler(recordingMessageHandler);
    }

    void TearDown() override
    {
        // A tester says what it found while the model it watches is still there, so the two are
        // torn down in that order and before the handler goes back to the logger.
        tester_.reset();
        model_.reset();

        qInstallMessageHandler(g_previous_handler);
        g_previous_handler = nullptr;
        g_contract_failures = nullptr;

        EXPECT_TRUE(contract_failures_.isEmpty())
            << contract_failures_.join('\n').toStdString();
    }

    TempHostListModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<TempHostListModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // Three hosts whose names and addresses only sort right when read as a person reads them.
    static QList<RouterTempHost> hosts()
    {
        return { makeHost(HostId(900000002), "host2", "192.168.0.2"),
                 makeHost(HostId(900000010), "host10", "192.168.0.10"),
                 makeHost(HostId(900000001), "alpha", "10.0.0.1") };
    }

    static QList<HostId> rowIds(const TempHostListModel* model)
    {
        QList<HostId> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->hostAt(i)->temp_id);
        return ids;
    }

    static QString textAt(const TempHostListModel* model, int row, Column column)
    {
        return model->index(row, static_cast<int>(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<TempHostListModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(TempHostListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 5);
    EXPECT_EQ(model()->hostAt(0), nullptr);
    EXPECT_EQ(model()->rowOf(HostId(1)), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(TempHostListModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("ID"));
    EXPECT_EQ(model()->headerData(4, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Address"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(TempHostListModelTest, RowShowsTheFieldsOfItsHost)
{
    model()->setHosts(hosts());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_EQ(textAt(model(), 0, Column::ID), QString("900000002"));
    EXPECT_EQ(textAt(model(), 0, Column::COMPUTER_NAME), QString("host2"));
    EXPECT_EQ(textAt(model(), 0, Column::OS), QString("Windows 11"));
    EXPECT_EQ(textAt(model(), 0, Column::VERSION), QString("2.7.0"));
    EXPECT_EQ(textAt(model(), 0, Column::ADDRESS), QString("192.168.0.2"));
}

//--------------------------------------------------------------------------------------------------
// The icon belongs to the column that names the row and is not repeated across it.
TEST_F(TempHostListModelTest, IconIsOnTheIdColumn)
{
    model()->setHosts(hosts());

    auto hasIcon = [this](int row, Column column)
    {
        return model()->index(row, static_cast<int>(column)).data(Qt::DecorationRole).isValid();
    };

    EXPECT_TRUE(hasIcon(0, Column::ID));
    EXPECT_FALSE(hasIcon(0, Column::COMPUTER_NAME));
    EXPECT_FALSE(hasIcon(0, Column::ADDRESS));
}

//--------------------------------------------------------------------------------------------------
// The id is a number, and comparing it as the text it is drawn as only happens to agree while every
// id is the same number of digits long.
TEST_F(TempHostListModelTest, IdIsSortedAsANumber)
{
    model()->setHosts(hosts());

    model()->sort(static_cast<int>(Column::ID), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()),
              QList<HostId>({ HostId(900000001), HostId(900000002), HostId(900000010) }));

    model()->sort(static_cast<int>(Column::ID), Qt::DescendingOrder);
    EXPECT_EQ(rowIds(model()),
              QList<HostId>({ HostId(900000010), HostId(900000002), HostId(900000001) }));
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "host2" comes before "host10".
TEST_F(TempHostListModelTest, NameIsSortedTheWayItIsRead)
{
    model()->setHosts(hosts());

    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model()),
              QList<HostId>({ HostId(900000001), HostId(900000002), HostId(900000010) }));
}

//--------------------------------------------------------------------------------------------------
// So is an address, so ".2" comes before ".10".
TEST_F(TempHostListModelTest, AddressIsSortedTheWayItIsRead)
{
    model()->setHosts(hosts());

    model()->sort(static_cast<int>(Column::ADDRESS), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model()),
              QList<HostId>({ HostId(900000001), HostId(900000002), HostId(900000010) }));
}

//--------------------------------------------------------------------------------------------------
// The list is refetched on every change, and it arrives sorted the way the user asked for without
// another click on the header.
TEST_F(TempHostListModelTest, NewListKeepsTheSortTheUserPicked)
{
    model()->setHosts(hosts());
    model()->sort(static_cast<int>(Column::ID), Qt::DescendingOrder);

    model()->setHosts(hosts());

    EXPECT_EQ(rowIds(model()),
              QList<HostId>({ HostId(900000010), HostId(900000002), HostId(900000001) }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another host - the
// one an approval would then act on.
TEST_F(TempHostListModelTest, SortingCarriesTheSelectionToItsHost)
{
    model()->setHosts(hosts());
    model()->sort(static_cast<int>(Column::ID), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->hostAt(selection.currentIndex().row())->temp_id, HostId(900000001));

    model()->sort(static_cast<int>(Column::ID), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->hostAt(selection.currentIndex().row())->temp_id, HostId(900000001));
}

//--------------------------------------------------------------------------------------------------
// The list is replaced whole on every refetch, so the row a host sits on is found by its id.
TEST_F(TempHostListModelTest, HostIsFoundByItsId)
{
    model()->setHosts(hosts());
    model()->sort(static_cast<int>(Column::ID), Qt::AscendingOrder);

    EXPECT_EQ(model()->rowOf(HostId(900000010)), 2);
    EXPECT_EQ(model()->hostAt(2)->temp_id, HostId(900000010));
    EXPECT_EQ(model()->rowOf(HostId(777)), -1);
}

//--------------------------------------------------------------------------------------------------
// The session of the router is gone, so what it answered is not shown as if it still held.
TEST_F(TempHostListModelTest, ClearDropsTheRows)
{
    model()->setHosts(hosts());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 5);
}
