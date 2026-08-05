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

#include "client/desktop/management/host_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

namespace {

using Column = HostListModel::Column;

// The layout of the host list of an administrator.
const QList<Column> kHostsLayout = { Column::HOST_ID, Column::DISPLAY_NAME, Column::COMPUTER_NAME,
                                     Column::ADDRESS, Column::USER_NAME, Column::COMMENT,
                                     Column::WORKSPACE, Column::OS, Column::VERSION, Column::ARCH,
                                     Column::LAST_CONNECT, Column::LAST_MODIFY, Column::STATUS };

// The layout of the host list of a group. Its first two columns are the other way round and it has
// no workspace of its own to name.
const QList<Column> kGroupLayout = { Column::DISPLAY_NAME, Column::HOST_ID, Column::COMPUTER_NAME,
                                     Column::ADDRESS, Column::USER_NAME, Column::COMMENT,
                                     Column::OS, Column::VERSION, Column::ARCH,
                                     Column::LAST_CONNECT, Column::LAST_MODIFY, Column::STATUS };

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
RouterHost makeHost(HostId host_id, const QString& display_name, const QString& computer_name)
{
    RouterHost host;
    host.host_id = host_id;
    host.display_name = display_name;
    host.computer_name = computer_name;
    return host;
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class HostListModelTest : public testing::Test
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
        testers_.clear();
        models_.clear();

        qInstallMessageHandler(g_previous_handler);
        g_previous_handler = nullptr;
        g_contract_failures = nullptr;

        EXPECT_TRUE(contract_failures_.isEmpty())
            << contract_failures_.join('\n').toStdString();
    }

    HostListModel* makeModel(const QList<Column>& columns)
    {
        auto model = std::make_unique<HostListModel>(columns);
        auto tester = std::make_unique<QAbstractItemModelTester>(
            model.get(), QAbstractItemModelTester::FailureReportingMode::Warning);

        HostListModel* result = model.get();

        models_.push_back(std::move(model));
        testers_.push_back(std::move(tester));

        return result;
    }

    // The three hosts every content test works with.
    static QList<RouterHost> hosts()
    {
        RouterHost first = makeHost(HostId(9), "host9", "alpha");
        first.address = "192.168.0.9";
        first.os_name = "Windows 11";
        first.version = "2.7.0";
        first.cpu_arch = "x86_64";
        first.user_name = "operator";
        first.comment = "spare";
        first.workspace_id = 10;
        first.last_connect = 1000;
        first.last_modify = 2000;
        first.online = true;

        RouterHost second = makeHost(HostId(10), "host10", "beta");
        second.workspace_id = 20;
        second.last_connect = 3000;

        RouterHost third = makeHost(HostId(2), QString(), "gamma");
        third.workspace_id = 10;

        return { first, second, third };
    }

    // Ids of the rows top to bottom, which is what a sort is judged by.
    static QList<HostId> rowIds(const HostListModel* model)
    {
        QList<HostId> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->hostAt(i)->host_id);
        return ids;
    }

    static QString textAt(const HostListModel* model, int row, Column column)
    {
        return model->index(row, model->sectionOf(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::vector<std::unique_ptr<QAbstractItemModelTester>> testers_;
    std::vector<std::unique_ptr<HostListModel>> models_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    HostListModel* model = makeModel(kHostsLayout);

    EXPECT_EQ(model->rowCount(), 0);
    EXPECT_EQ(model->columnCount(), static_cast<int>(kHostsLayout.size()));
    EXPECT_EQ(model->hostAt(0), nullptr);
    EXPECT_EQ(model->rowOf(HostId(1)), -1);
}

//--------------------------------------------------------------------------------------------------
// The two views show the same records in a different order, which is the whole reason the layout is
// given to the model instead of being fixed in it.
TEST_F(HostListModelTest, LayoutDecidesTheColumnsAndTheirOrder)
{
    HostListModel* hosts_model = makeModel(kHostsLayout);
    HostListModel* group_model = makeModel(kGroupLayout);

    EXPECT_EQ(hosts_model->columnAt(0), Column::HOST_ID);
    EXPECT_EQ(hosts_model->columnAt(1), Column::DISPLAY_NAME);
    EXPECT_EQ(group_model->columnAt(0), Column::DISPLAY_NAME);
    EXPECT_EQ(group_model->columnAt(1), Column::HOST_ID);

    // A column the view does not show has no section of its own.
    EXPECT_GE(hosts_model->sectionOf(Column::WORKSPACE), 0);
    EXPECT_EQ(group_model->sectionOf(Column::WORKSPACE), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, HeaderNamesTheColumnOfItsSection)
{
    HostListModel* model = makeModel(kGroupLayout);

    EXPECT_EQ(model->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Display Name"));
    EXPECT_EQ(model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Host ID"));

    // A vertical header is not ours to fill, and a section outside the layout has no name.
    EXPECT_FALSE(model->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model->headerData(99, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, RowShowsTheFieldsOfItsHost)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    ASSERT_EQ(model->rowCount(), 3);

    EXPECT_EQ(textAt(model, 0, Column::HOST_ID), QString("9"));
    EXPECT_EQ(textAt(model, 0, Column::DISPLAY_NAME), QString("host9"));
    EXPECT_EQ(textAt(model, 0, Column::COMPUTER_NAME), QString("alpha"));
    EXPECT_EQ(textAt(model, 0, Column::ADDRESS), QString("192.168.0.9"));
    EXPECT_EQ(textAt(model, 0, Column::USER_NAME), QString("operator"));
    EXPECT_EQ(textAt(model, 0, Column::COMMENT), QString("spare"));
    EXPECT_EQ(textAt(model, 0, Column::OS), QString("Windows 11"));
    EXPECT_EQ(textAt(model, 0, Column::VERSION), QString("2.7.0"));
    EXPECT_EQ(textAt(model, 0, Column::ARCH), QString("x86_64"));
}

//--------------------------------------------------------------------------------------------------
// A host is named by the administrator, and until then by the computer it runs on.
TEST_F(HostListModelTest, UnnamedHostIsShownByItsComputerName)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    EXPECT_EQ(textAt(model, 2, Column::DISPLAY_NAME), QString("gamma"));
}

//--------------------------------------------------------------------------------------------------
// The names of the workspaces come from a list of their own, which arrives whenever it arrives.
TEST_F(HostListModelTest, WorkspaceColumnIsEmptyUntilTheNamesAreKnown)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    EXPECT_TRUE(textAt(model, 0, Column::WORKSPACE).isEmpty());

    model->setWorkspaceNames({ { 10, "alpha" }, { 20, "beta" } });

    EXPECT_EQ(textAt(model, 0, Column::WORKSPACE), QString("alpha"));
    EXPECT_EQ(textAt(model, 1, Column::WORKSPACE), QString("beta"));
}

//--------------------------------------------------------------------------------------------------
// Whether the host is connected is drawn once per row, on the leftmost column that names the host.
// The two views name it by different columns, so the icon lands on a different section in each.
TEST_F(HostListModelTest, StateIconIsOnTheLeftmostNamingColumn)
{
    HostListModel* hosts_model = makeModel(kHostsLayout);
    HostListModel* group_model = makeModel(kGroupLayout);

    hosts_model->setHosts(hosts());
    group_model->setHosts(hosts());

    auto hasIcon = [](const HostListModel* model, int row, int section)
    {
        return model->index(row, section).data(Qt::DecorationRole).isValid();
    };

    EXPECT_TRUE(hasIcon(hosts_model, 0, hosts_model->sectionOf(Column::HOST_ID)));
    EXPECT_FALSE(hasIcon(hosts_model, 0, hosts_model->sectionOf(Column::DISPLAY_NAME)));

    EXPECT_TRUE(hasIcon(group_model, 0, group_model->sectionOf(Column::DISPLAY_NAME)));
    EXPECT_FALSE(hasIcon(group_model, 0, group_model->sectionOf(Column::HOST_ID)));
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, StatusFollowsTheHost)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    EXPECT_EQ(textAt(model, 0, Column::STATUS), QString("Online"));
    EXPECT_EQ(textAt(model, 1, Column::STATUS), QString("Offline"));
}

//--------------------------------------------------------------------------------------------------
// A host that never connected has no time to show, and zero is not a time.
TEST_F(HostListModelTest, UnsetTimestampIsShownAsNothing)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    EXPECT_FALSE(textAt(model, 0, Column::LAST_CONNECT).isEmpty());
    EXPECT_TRUE(textAt(model, 1, Column::LAST_MODIFY).isEmpty());
    EXPECT_TRUE(textAt(model, 2, Column::LAST_CONNECT).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The id is a number, so 9 comes before 10. Sorting it as the text it is drawn as would not.
TEST_F(HostListModelTest, IdIsSortedAsANumber)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    model->sort(model->sectionOf(Column::HOST_ID), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(2), HostId(9), HostId(10) }));

    model->sort(model->sectionOf(Column::HOST_ID), Qt::DescendingOrder);
    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(10), HostId(9), HostId(2) }));
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "host9" comes before "host10" there as well. The name of an
// unnamed host is the one it is shown by.
TEST_F(HostListModelTest, NameIsSortedTheWayItIsRead)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    model->sort(model->sectionOf(Column::DISPLAY_NAME), Qt::AscendingOrder);

    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(2), HostId(9), HostId(10) }));
    EXPECT_EQ(textAt(model, 0, Column::DISPLAY_NAME), QString("gamma"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, TimestampIsSortedAsATime)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    model->sort(model->sectionOf(Column::LAST_CONNECT), Qt::AscendingOrder);

    // The host that never connected is the earliest of them.
    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(2), HostId(9), HostId(10) }));
}

//--------------------------------------------------------------------------------------------------
// A column with nothing special about it is sorted by what it shows.
TEST_F(HostListModelTest, PlainColumnIsSortedByItsText)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    model->sort(model->sectionOf(Column::COMPUTER_NAME), Qt::DescendingOrder);

    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(2), HostId(10), HostId(9) }));
}

//--------------------------------------------------------------------------------------------------
// The next page of the list arrives sorted the way the user asked for, without another click on
// the header.
TEST_F(HostListModelTest, NewPageKeepsTheSortTheUserPicked)
{
    HostListModel* model = makeModel(kHostsLayout);

    model->setHosts(hosts());
    model->sort(model->sectionOf(Column::HOST_ID), Qt::DescendingOrder);

    model->setHosts(hosts());

    EXPECT_EQ(rowIds(model), QList<HostId>({ HostId(10), HostId(9), HostId(2) }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another host - the
// one a context menu would then act on.
TEST_F(HostListModelTest, SortingCarriesTheSelectionToItsHost)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());
    model->sort(model->sectionOf(Column::HOST_ID), Qt::AscendingOrder);

    QItemSelectionModel selection(model);
    selection.setCurrentIndex(model->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model->hostAt(selection.currentIndex().row())->host_id, HostId(2));

    model->sort(model->sectionOf(Column::HOST_ID), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model->hostAt(selection.currentIndex().row())->host_id, HostId(2));
}

//--------------------------------------------------------------------------------------------------
// The page is replaced whole on every refetch, so the row a host sits on is found by its id.
TEST_F(HostListModelTest, HostIsFoundByItsId)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());
    model->sort(model->sectionOf(Column::HOST_ID), Qt::AscendingOrder);

    EXPECT_EQ(model->rowOf(HostId(10)), 2);
    EXPECT_EQ(model->hostAt(2)->host_id, HostId(10));
    EXPECT_EQ(model->rowOf(HostId(777)), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListModelTest, EmptyPageClearsTheRows)
{
    HostListModel* model = makeModel(kHostsLayout);
    model->setHosts(hosts());

    model->setHosts({});

    EXPECT_EQ(model->rowCount(), 0);
    EXPECT_EQ(model->columnCount(), static_cast<int>(kHostsLayout.size()));
}
