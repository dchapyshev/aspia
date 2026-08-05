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

#include "client/desktop/management/local_host_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"

namespace {

using Column = LocalHostListModel::Column;

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
HostConfig makeHost(qint64 id, const QString& name, const QString& address, qint64 create_time)
{
    HostConfig host;
    host.setId(id);
    host.setName(name);
    host.setAddress(address);
    host.setCreateTime(create_time);
    return host;
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class LocalHostListModelTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // A record of the address book keeps its text encrypted with a key of the process, which
        // the application sets once the master password is entered. Without one nothing can be
        // written into a record at all.
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

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

    LocalHostListModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<LocalHostListModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // Three hosts whose names and addresses only sort right when read as a person reads them, and
    // which were created in an order the names do not follow.
    static QList<HostConfig> hosts()
    {
        return { makeHost(1, "host2", "192.168.0.2", 3000),
                 makeHost(2, "host10", "192.168.0.10", 1000),
                 makeHost(3, "alpha", "10.0.0.1", 2000) };
    }

    static QList<qint64> rowIds(const LocalHostListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->hostAt(i)->id());
        return ids;
    }

    static QString textAt(const LocalHostListModel* model, int row, Column column)
    {
        return model->index(row, static_cast<int>(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<LocalHostListModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 7);
    EXPECT_EQ(model()->hostAt(0), nullptr);
    EXPECT_EQ(model()->rowOf(1), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostListModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Name"));
    EXPECT_EQ(model()->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Address / ID"));
    EXPECT_EQ(model()->headerData(6, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Status"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostListModelTest, RowShowsTheFieldsOfItsHost)
{
    model()->setHosts(hosts());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_EQ(textAt(model(), 0, Column::NAME), QString("host2"));
    EXPECT_EQ(textAt(model(), 0, Column::ADDRESS), QString("192.168.0.2"));
    EXPECT_FALSE(textAt(model(), 0, Column::CREATED).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host that was never connected to has no time to show, and zero is not a time.
TEST_F(LocalHostListModelTest, UnsetTimestampIsShownAsNothing)
{
    model()->setHosts(hosts());

    EXPECT_TRUE(textAt(model(), 0, Column::CONNECT).isEmpty());
    EXPECT_TRUE(textAt(model(), 0, Column::MODIFIED).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A comment of several lines would stretch the row it sits on, so it is drawn on one line and the
// whole of it is only in the tooltip.
TEST_F(LocalHostListModelTest, CommentIsDrawnOnOneLineAndKeptWholeInTheTooltip)
{
    HostConfig host = makeHost(1, "host", "192.168.0.1", 1000);
    host.setComment("first\nsecond");

    model()->setHosts({ host });

    EXPECT_EQ(textAt(model(), 0, Column::COMMENT), QString("first second"));
    EXPECT_EQ(model()->index(0, static_cast<int>(Column::COMMENT)).data(Qt::ToolTipRole).toString(),
              QString("first\nsecond"));

    // The tooltip belongs to that column alone.
    EXPECT_FALSE(model()->index(0, static_cast<int>(Column::NAME)).data(Qt::ToolTipRole).isValid());
}

//--------------------------------------------------------------------------------------------------
// Until a host is probed it is neither online nor offline, and the row says nothing about it.
TEST_F(LocalHostListModelTest, StatusIsEmptyUntilTheHostIsProbed)
{
    model()->setHosts(hosts());

    EXPECT_TRUE(textAt(model(), 0, Column::STATUS).isEmpty());

    model()->setOnlineStatus(1, true);
    model()->setOnlineStatus(2, false);

    EXPECT_EQ(textAt(model(), 0, Column::STATUS), QString("Online"));
    EXPECT_EQ(textAt(model(), 1, Column::STATUS), QString("Offline"));
    EXPECT_TRUE(textAt(model(), 2, Column::STATUS).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Another probe starts over, so what the previous one found is not left standing.
TEST_F(LocalHostListModelTest, ClearingTheStatusesForgetsWhatWasProbed)
{
    model()->setHosts(hosts());
    model()->setOnlineStatus(1, true);

    model()->clearOnlineStatuses();

    EXPECT_TRUE(textAt(model(), 0, Column::STATUS).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The three states of a host look different, and the icon belongs to the column that names it.
TEST_F(LocalHostListModelTest, IconTellsTheStateApart)
{
    model()->setHosts(hosts());

    auto icon = [this](int row)
    {
        return model()->index(row, static_cast<int>(Column::NAME))
            .data(Qt::DecorationRole).value<QIcon>().cacheKey();
    };

    const qint64 unknown = icon(0);

    model()->setOnlineStatus(1, true);
    const qint64 online = icon(0);

    model()->setOnlineStatus(1, false);
    const qint64 offline = icon(0);

    EXPECT_NE(unknown, online);
    EXPECT_NE(online, offline);
    EXPECT_NE(unknown, offline);

    EXPECT_FALSE(model()->index(0, static_cast<int>(Column::STATUS))
                     .data(Qt::DecorationRole).isValid());
}

//--------------------------------------------------------------------------------------------------
// Another group is another list, and the states probed for the previous one do not carry over.
TEST_F(LocalHostListModelTest, AnotherGroupForgetsWhatWasProbed)
{
    model()->setHosts(hosts());
    model()->setOnlineStatus(1, true);

    model()->setHosts(hosts());

    EXPECT_TRUE(textAt(model(), 0, Column::STATUS).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "host2" comes before "host10". So is an address.
TEST_F(LocalHostListModelTest, NameAndAddressAreSortedTheWayTheyAreRead)
{
    model()->setHosts(hosts());

    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));

    model()->sort(static_cast<int>(Column::ADDRESS), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 3, 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// The moments in time are compared as the times they are and not as the text they are drawn as.
TEST_F(LocalHostListModelTest, TimestampsAreSortedAsTimes)
{
    model()->setHosts(hosts());

    model()->sort(static_cast<int>(Column::CREATED), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 2, 3, 1 }));
}

//--------------------------------------------------------------------------------------------------
// A record is edited one at a time here, not replaced with the whole list.
TEST_F(LocalHostListModelTest, EditedRecordCatchesUp)
{
    model()->setHosts(hosts());

    HostConfig edited = makeHost(1, "renamed", "192.168.0.99", 3000);
    EXPECT_TRUE(model()->updateHost(edited));

    EXPECT_EQ(textAt(model(), 0, Column::NAME), QString("renamed"));
    EXPECT_EQ(textAt(model(), 0, Column::ADDRESS), QString("192.168.0.99"));

    // A record of another group is not ours to show.
    EXPECT_FALSE(model()->updateHost(makeHost(777, "other", "10.0.0.7", 1000)));
}

//--------------------------------------------------------------------------------------------------
// A connection was just made, which is the only field of a record this list writes by itself.
TEST_F(LocalHostListModelTest, ConnectTimeIsWrittenToItsRow)
{
    model()->setHosts(hosts());

    ASSERT_TRUE(textAt(model(), 0, Column::CONNECT).isEmpty());

    model()->setConnectTime(1, 5000);

    EXPECT_FALSE(textAt(model(), 0, Column::CONNECT).isEmpty());
    EXPECT_EQ(model()->hostAt(0)->connectTime(), 5000);
}

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostListModelTest, RemovedRecordLeavesTheList)
{
    model()->setHosts(hosts());

    EXPECT_TRUE(model()->removeHost(2));

    EXPECT_EQ(model()->rowCount(), 2);
    EXPECT_EQ(model()->rowOf(2), -1);
    EXPECT_FALSE(model()->removeHost(777));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another host - the
// one a delete would then act on.
TEST_F(LocalHostListModelTest, SortingCarriesTheSelectionToItsHost)
{
    model()->setHosts(hosts());
    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->hostAt(selection.currentIndex().row())->id(), 3);

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->hostAt(selection.currentIndex().row())->id(), 3);
}

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostListModelTest, ClearDropsTheRows)
{
    model()->setHosts(hosts());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 7);
}
