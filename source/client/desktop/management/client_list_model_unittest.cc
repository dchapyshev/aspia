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

#include "client/desktop/management/client_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "proto/peer.h"

namespace {

using Column = ClientListModel::Column;

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
void addClient(proto::router::ClientList* list, qint64 entry_id, const QString& computer_name,
               qint64 timepoint)
{
    proto::router::ClientInfo* client = list->add_client();
    client->set_entry_id(entry_id);
    client->set_computer_name(computer_name.toStdString());
    client->set_ip_address("192.168.0.1");
    client->set_architecture("x86_64");
    client->set_os_name("Windows 11");
    client->set_timepoint(timepoint);

    proto::peer::Version* version = client->mutable_version();
    version->set_major(2);
    version->set_minor(7);
    version->set_patch(0);
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class ClientListModelTest : public testing::Test
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

    ClientListModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<ClientListModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // Three sessions whose names only sort right when read as a person reads them, and which
    // connected in an order the names do not follow.
    static proto::router::ClientList clients()
    {
        proto::router::ClientList list;

        addClient(&list, 1, "host2", 3000);
        addClient(&list, 2, "host10", 1000);
        addClient(&list, 3, "alpha", 2000);

        return list;
    }

    // Ids of the rows top to bottom, which is what a sort is judged by.
    static QList<qint64> rowIds(const ClientListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->clientAt(i)->entry_id());
        return ids;
    }

    static QString textAt(const ClientListModel* model, int row, Column column)
    {
        return model->index(row, static_cast<int>(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<ClientListModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(ClientListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 6);
    EXPECT_EQ(model()->clientAt(0), nullptr);
    EXPECT_EQ(model()->rowOf(1), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ClientListModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Computer Name"));
    EXPECT_EQ(model()->headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Connect Time"));
    EXPECT_EQ(model()->headerData(5, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Operating System"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(ClientListModelTest, RowShowsTheFieldsOfItsSession)
{
    model()->setClients(clients());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_EQ(textAt(model(), 0, Column::COMPUTER_NAME), QString("host2"));
    EXPECT_EQ(textAt(model(), 0, Column::IP_ADDRESS), QString("192.168.0.1"));
    EXPECT_EQ(textAt(model(), 0, Column::ARCHITECTURE), QString("x86_64"));
    EXPECT_EQ(textAt(model(), 0, Column::OS), QString("Windows 11"));
    EXPECT_FALSE(textAt(model(), 0, Column::CONNECT_TIME).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The three parts of a version are what the peer reported, and the column puts them together.
TEST_F(ClientListModelTest, VersionIsShownAsOneValue)
{
    model()->setClients(clients());

    EXPECT_EQ(textAt(model(), 0, Column::VERSION), QString("2.7.0"));
}

//--------------------------------------------------------------------------------------------------
// The icon belongs to the column that names the machine and is not repeated across the row.
TEST_F(ClientListModelTest, IconIsOnTheNameColumn)
{
    model()->setClients(clients());

    auto hasIcon = [this](int row, Column column)
    {
        return model()->index(row, static_cast<int>(column)).data(Qt::DecorationRole).isValid();
    };

    EXPECT_TRUE(hasIcon(0, Column::COMPUTER_NAME));
    EXPECT_FALSE(hasIcon(0, Column::IP_ADDRESS));
    EXPECT_FALSE(hasIcon(0, Column::OS));
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "host2" comes before "host10".
TEST_F(ClientListModelTest, NameIsSortedTheWayItIsRead)
{
    model()->setClients(clients());

    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));

    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::DescendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 2, 1, 3 }));
}

//--------------------------------------------------------------------------------------------------
// The moment of the connection is compared as the time it is. Sorting it as the text it is drawn as
// would sort by the order the parts of a date happen to be written in.
TEST_F(ClientListModelTest, ConnectTimeIsSortedAsATime)
{
    model()->setClients(clients());

    model()->sort(static_cast<int>(Column::CONNECT_TIME), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 2, 3, 1 }));
}

//--------------------------------------------------------------------------------------------------
// A column with nothing special about it is sorted by what it shows, and records that read the same
// keep the order they came in.
TEST_F(ClientListModelTest, PlainColumnIsSortedByItsTextAndKeepsTheRestInPlace)
{
    model()->setClients(clients());

    model()->sort(static_cast<int>(Column::OS), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 2, 3 }));
}

//--------------------------------------------------------------------------------------------------
// The list is refetched on every change, and it arrives sorted the way the user asked for without
// another click on the header.
TEST_F(ClientListModelTest, NewListKeepsTheSortTheUserPicked)
{
    model()->setClients(clients());
    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::AscendingOrder);

    model()->setClients(clients());

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another session -
// the one a disconnect would then act on.
TEST_F(ClientListModelTest, SortingCarriesTheSelectionToItsSession)
{
    model()->setClients(clients());
    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->clientAt(selection.currentIndex().row())->entry_id(), 3);

    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->clientAt(selection.currentIndex().row())->entry_id(), 3);
}

//--------------------------------------------------------------------------------------------------
// The list is replaced whole on every refetch, so the row a session sits on is found by its id.
TEST_F(ClientListModelTest, SessionIsFoundByItsId)
{
    model()->setClients(clients());
    model()->sort(static_cast<int>(Column::COMPUTER_NAME), Qt::AscendingOrder);

    EXPECT_EQ(model()->rowOf(2), 2);
    EXPECT_EQ(model()->clientAt(2)->entry_id(), 2);
    EXPECT_EQ(model()->rowOf(777), -1);
}

//--------------------------------------------------------------------------------------------------
// The session of the router is gone, so what it answered is not shown as if it still held.
TEST_F(ClientListModelTest, ClearDropsTheRows)
{
    model()->setClients(clients());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 6);
}
