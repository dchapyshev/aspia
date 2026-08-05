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

#include "client/desktop/management/relay_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "client/desktop/management/peer_list_model.h"
#include "proto/peer.h"

namespace {

using RelayColumn = RelayListModel::Column;
using PeerColumn = PeerListModel::Column;

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
proto::router::RelayInfo* addRelay(proto::router::RelayList* list, qint64 entry_id,
                                   const QString& address, qint64 timepoint, quint32 pool_size)
{
    proto::router::RelayInfo* relay = list->add_relay();
    relay->set_entry_id(entry_id);
    relay->set_ip_address(address.toStdString());
    relay->set_timepoint(timepoint);
    relay->set_pool_size(pool_size);
    relay->set_computer_name("relay");
    relay->set_architecture("x86_64");
    relay->set_os_name("Windows 11");

    proto::peer::Version* version = relay->mutable_version();
    version->set_major(2);
    version->set_minor(7);
    version->set_patch(0);

    return relay;
}

//--------------------------------------------------------------------------------------------------
void addPeer(proto::router::RelayInfo::Statistics* statistics, qint64 peer_id,
             const QString& user_name, quint64 host_id, quint64 bytes, quint64 duration,
             quint64 idle)
{
    proto::router::Peer* peer = statistics->add_peer();
    peer->set_peer_id(peer_id);
    peer->set_client_user_name(user_name.toStdString());
    peer->set_host_id(host_id);
    peer->set_host_address("192.168.0.1");
    peer->set_client_address("192.168.0.2");
    peer->set_bytes_transferred(bytes);
    peer->set_duration(duration);
    peer->set_idle_time(idle);
}

} // namespace

// Both models of the fixture are watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class RelayListModelTest : public testing::Test
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
        relay_tester_.reset();
        peer_tester_.reset();
        relay_model_.reset();
        peer_model_.reset();

        qInstallMessageHandler(g_previous_handler);
        g_previous_handler = nullptr;
        g_contract_failures = nullptr;

        EXPECT_TRUE(contract_failures_.isEmpty())
            << contract_failures_.join('\n').toStdString();
    }

    RelayListModel* relayModel()
    {
        if (!relay_model_)
        {
            relay_model_ = std::make_unique<RelayListModel>();
            relay_tester_ = std::make_unique<QAbstractItemModelTester>(
                relay_model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return relay_model_.get();
    }

    PeerListModel* peerModel()
    {
        if (!peer_model_)
        {
            peer_model_ = std::make_unique<PeerListModel>();
            peer_tester_ = std::make_unique<QAbstractItemModelTester>(
                peer_model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return peer_model_.get();
    }

    // Three relays whose addresses only sort right when read as a person reads them, and which
    // connected in an order the addresses do not follow.
    static proto::router::RelayList relays()
    {
        proto::router::RelayList list;

        addRelay(&list, 1, "192.168.0.2", 3000, 9);
        addRelay(&list, 2, "192.168.0.10", 1000, 10);
        addRelay(&list, 3, "10.0.0.1", 2000, 100);

        return list;
    }

    static proto::router::RelayInfo::Statistics peers()
    {
        proto::router::RelayInfo::Statistics statistics;

        addPeer(&statistics, 1, "user2", 900, 2048, 30, 5);
        addPeer(&statistics, 2, "user10", 100, 1048576, 10, 60);

        return statistics;
    }

    static QList<qint64> relayIds(const RelayListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->relayAt(i)->entry_id());
        return ids;
    }

    static QList<qint64> peerIds(const PeerListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->peerAt(i)->peer_id());
        return ids;
    }

private:
    std::unique_ptr<QAbstractItemModelTester> relay_tester_;
    std::unique_ptr<QAbstractItemModelTester> peer_tester_;
    std::unique_ptr<RelayListModel> relay_model_;
    std::unique_ptr<PeerListModel> peer_model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, EmptyModelsHaveTheirColumnsAndNoRows)
{
    EXPECT_EQ(relayModel()->rowCount(), 0);
    EXPECT_EQ(relayModel()->columnCount(), 7);
    EXPECT_EQ(relayModel()->relayAt(0), nullptr);
    EXPECT_EQ(relayModel()->rowOf(1), -1);

    EXPECT_EQ(peerModel()->rowCount(), 0);
    EXPECT_EQ(peerModel()->columnCount(), 7);
    EXPECT_EQ(peerModel()->peerAt(0), nullptr);
    EXPECT_EQ(peerModel()->rowOf(1), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, HeadersNameTheColumnsOfTheirSections)
{
    EXPECT_EQ(relayModel()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Address"));
    EXPECT_EQ(relayModel()->headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Pool Size"));

    EXPECT_EQ(peerModel()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("User Name"));
    EXPECT_EQ(peerModel()->headerData(4, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Transferred"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(relayModel()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(peerModel()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, RelayRowShowsItsFields)
{
    relayModel()->setRelays(relays());

    ASSERT_EQ(relayModel()->rowCount(), 3);

    auto textAt = [this](int row, RelayColumn column)
    {
        return relayModel()->index(row, static_cast<int>(column)).data().toString();
    };

    EXPECT_EQ(textAt(0, RelayColumn::ADDRESS), QString("192.168.0.2"));
    EXPECT_EQ(textAt(0, RelayColumn::POOL_SIZE), QString("9"));
    EXPECT_EQ(textAt(0, RelayColumn::VERSION), QString("2.7.0"));
    EXPECT_EQ(textAt(0, RelayColumn::OS), QString("Windows 11"));
    EXPECT_FALSE(textAt(0, RelayColumn::CONNECT_TIME).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Bytes and spans of time are drawn as rounded amounts, which is what the user reads.
TEST_F(RelayListModelTest, PeerRowShowsAmountsTheWayTheyAreRead)
{
    peerModel()->setPeers(peers());

    ASSERT_EQ(peerModel()->rowCount(), 2);

    auto textAt = [this](int row, PeerColumn column)
    {
        return peerModel()->index(row, static_cast<int>(column)).data().toString();
    };

    EXPECT_EQ(textAt(0, PeerColumn::USER_NAME), QString("user2"));
    EXPECT_EQ(textAt(0, PeerColumn::HOST_ID), QString("900"));
    EXPECT_FALSE(textAt(0, PeerColumn::TRANSFERRED).isEmpty());
    EXPECT_FALSE(textAt(0, PeerColumn::DURATION).isEmpty());
    EXPECT_NE(textAt(0, PeerColumn::TRANSFERRED), textAt(1, PeerColumn::TRANSFERRED));
}

//--------------------------------------------------------------------------------------------------
// The icon belongs to the column that names the row and is not repeated across it.
TEST_F(RelayListModelTest, IconsAreOnTheNamingColumns)
{
    relayModel()->setRelays(relays());
    peerModel()->setPeers(peers());

    EXPECT_TRUE(relayModel()->index(0, static_cast<int>(RelayColumn::ADDRESS))
                    .data(Qt::DecorationRole).isValid());
    EXPECT_FALSE(relayModel()->index(0, static_cast<int>(RelayColumn::OS))
                     .data(Qt::DecorationRole).isValid());

    EXPECT_TRUE(peerModel()->index(0, static_cast<int>(PeerColumn::USER_NAME))
                    .data(Qt::DecorationRole).isValid());
    EXPECT_FALSE(peerModel()->index(0, static_cast<int>(PeerColumn::HOST_ID))
                     .data(Qt::DecorationRole).isValid());
}

//--------------------------------------------------------------------------------------------------
// An address is read by a person, so ".2" comes before ".10".
TEST_F(RelayListModelTest, AddressIsSortedTheWayItIsRead)
{
    relayModel()->setRelays(relays());

    relayModel()->sort(static_cast<int>(RelayColumn::ADDRESS), Qt::AscendingOrder);

    EXPECT_EQ(relayIds(relayModel()), QList<qint64>({ 3, 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, ConnectTimeIsSortedAsATime)
{
    relayModel()->setRelays(relays());

    relayModel()->sort(static_cast<int>(RelayColumn::CONNECT_TIME), Qt::AscendingOrder);

    EXPECT_EQ(relayIds(relayModel()), QList<qint64>({ 2, 3, 1 }));
}

//--------------------------------------------------------------------------------------------------
// The size of the pool is a number, so 9 comes before 10 and 10 before 100. Sorting it as the text
// it is drawn as would put 10 first.
TEST_F(RelayListModelTest, PoolSizeIsSortedAsANumber)
{
    relayModel()->setRelays(relays());

    relayModel()->sort(static_cast<int>(RelayColumn::POOL_SIZE), Qt::AscendingOrder);

    EXPECT_EQ(relayIds(relayModel()), QList<qint64>({ 1, 2, 3 }));
}

//--------------------------------------------------------------------------------------------------
// What is drawn as a rounded amount is compared by the value it was rounded from, or "1 MB" would
// come before "2 KB".
TEST_F(RelayListModelTest, TransferredIsSortedByItsBytes)
{
    peerModel()->setPeers(peers());

    peerModel()->sort(static_cast<int>(PeerColumn::TRANSFERRED), Qt::AscendingOrder);

    EXPECT_EQ(peerIds(peerModel()), QList<qint64>({ 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, DurationAndIdleAreSortedByTheirSeconds)
{
    peerModel()->setPeers(peers());

    peerModel()->sort(static_cast<int>(PeerColumn::DURATION), Qt::AscendingOrder);
    EXPECT_EQ(peerIds(peerModel()), QList<qint64>({ 2, 1 }));

    peerModel()->sort(static_cast<int>(PeerColumn::IDLE), Qt::AscendingOrder);
    EXPECT_EQ(peerIds(peerModel()), QList<qint64>({ 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// An id is a number as well, so 100 comes before 900.
TEST_F(RelayListModelTest, HostIdIsSortedAsANumber)
{
    peerModel()->setPeers(peers());

    peerModel()->sort(static_cast<int>(PeerColumn::HOST_ID), Qt::AscendingOrder);

    EXPECT_EQ(peerIds(peerModel()), QList<qint64>({ 2, 1 }));
}

//--------------------------------------------------------------------------------------------------
// The list is refetched on every change, and it arrives sorted the way the user asked for without
// another click on the header.
TEST_F(RelayListModelTest, NewListKeepsTheSortTheUserPicked)
{
    relayModel()->setRelays(relays());
    relayModel()->sort(static_cast<int>(RelayColumn::POOL_SIZE), Qt::DescendingOrder);

    relayModel()->setRelays(relays());

    EXPECT_EQ(relayIds(relayModel()), QList<qint64>({ 3, 2, 1 }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another relay -
// the one a disconnect would then act on.
TEST_F(RelayListModelTest, SortingCarriesTheSelectionToItsRelay)
{
    relayModel()->setRelays(relays());
    relayModel()->sort(static_cast<int>(RelayColumn::POOL_SIZE), Qt::AscendingOrder);

    QItemSelectionModel selection(relayModel());
    selection.setCurrentIndex(relayModel()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(relayModel()->relayAt(selection.currentIndex().row())->entry_id(), 1);

    relayModel()->sort(static_cast<int>(RelayColumn::POOL_SIZE), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(relayModel()->relayAt(selection.currentIndex().row())->entry_id(), 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RelayListModelTest, SortingCarriesTheSelectionToItsPeer)
{
    peerModel()->setPeers(peers());
    peerModel()->sort(static_cast<int>(PeerColumn::HOST_ID), Qt::AscendingOrder);

    QItemSelectionModel selection(peerModel());
    selection.setCurrentIndex(peerModel()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(peerModel()->peerAt(selection.currentIndex().row())->peer_id(), 2);

    peerModel()->sort(static_cast<int>(PeerColumn::HOST_ID), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(peerModel()->peerAt(selection.currentIndex().row())->peer_id(), 2);
}

//--------------------------------------------------------------------------------------------------
// The list is replaced whole on every refetch, so the row a record sits on is found by its id.
TEST_F(RelayListModelTest, RecordsAreFoundByTheirIds)
{
    relayModel()->setRelays(relays());
    relayModel()->sort(static_cast<int>(RelayColumn::POOL_SIZE), Qt::AscendingOrder);

    EXPECT_EQ(relayModel()->rowOf(3), 2);
    EXPECT_EQ(relayModel()->relayAt(2)->entry_id(), 3);
    EXPECT_EQ(relayModel()->rowOf(777), -1);

    peerModel()->setPeers(peers());
    EXPECT_EQ(peerModel()->rowOf(777), -1);
}

//--------------------------------------------------------------------------------------------------
// The session of the router is gone, so what it answered is not shown as if it still held.
TEST_F(RelayListModelTest, ClearDropsTheRows)
{
    relayModel()->setRelays(relays());
    peerModel()->setPeers(peers());

    relayModel()->clear();
    peerModel()->clear();

    EXPECT_EQ(relayModel()->rowCount(), 0);
    EXPECT_EQ(relayModel()->columnCount(), 7);
    EXPECT_EQ(peerModel()->rowCount(), 0);
    EXPECT_EQ(peerModel()->columnCount(), 7);
}
