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

#include "client/desktop/management/search_result_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"

namespace {

using Column = SearchResultModel::Column;
using Row = SearchResultModel::Row;
using Type = SearchResultModel::Type;

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
Row makeLocalRow(qint64 id, const QString& name, const QString& address, const QString& group_path)
{
    Row row;
    row.type = Type::LOCAL;
    row.source = group_path;
    row.host.setId(id);
    row.host.setName(name);
    row.host.setAddress(address);
    return row;
}

//--------------------------------------------------------------------------------------------------
// A router host is never stored, so its record keeps the id a record never has.
Row makeRouterRow(const QString& name, const QString& address, const QString& label)
{
    Row row;
    row.type = Type::ROUTER;
    row.source = label;
    row.host.setName(name);
    row.host.setAddress(address);
    return row;
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class SearchResultModelTest : public testing::Test
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

    SearchResultModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<SearchResultModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // A page as the widget builds it: the matches of the address book first, then the ones of a
    // router. The names only sort right when read as a person reads them.
    static QList<Row> rows()
    {
        return { makeLocalRow(11, "host2", "192.168.0.2", "Local / Office"),
                 makeLocalRow(12, "host10", "192.168.0.10", "Local"),
                 makeRouterRow("alpha", "123 456 789", "Router 1") };
    }

    static QStringList rowNames(const SearchResultModel* model)
    {
        QStringList names;
        for (int i = 0; i < model->rowCount(); ++i)
            names.append(model->rowAt(i)->host.name());
        return names;
    }

    static QString textAt(const SearchResultModel* model, int row, Column column)
    {
        return model->index(row, static_cast<int>(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<SearchResultModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(SearchResultModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 4);
    EXPECT_EQ(model()->rowAt(0), nullptr);
    EXPECT_EQ(model()->rowOfEntry(11), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SearchResultModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Name"));
    EXPECT_EQ(model()->headerData(3, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Comment"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
// A row of either kind is shown the same way; what differs is where the third column comes from.
TEST_F(SearchResultModelTest, RowShowsWhereItWasFound)
{
    model()->setRows(rows());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_EQ(textAt(model(), 0, Column::NAME), QString("host2"));
    EXPECT_EQ(textAt(model(), 0, Column::ADDRESS), QString("192.168.0.2"));
    EXPECT_EQ(textAt(model(), 0, Column::SOURCE), QString("Local / Office"));

    EXPECT_EQ(textAt(model(), 2, Column::NAME), QString("alpha"));
    EXPECT_EQ(textAt(model(), 2, Column::ADDRESS), QString("123 456 789"));
    EXPECT_EQ(textAt(model(), 2, Column::SOURCE), QString("Router 1"));
}

//--------------------------------------------------------------------------------------------------
// The row is one line high, so a comment of several lines is drawn as one and read whole in the
// tooltip. The path of a group is elided rather than wrapped, so it has a tooltip as well.
TEST_F(SearchResultModelTest, WhatDoesNotFitOnTheRowIsInTheTooltip)
{
    Row row = makeLocalRow(11, "host", "192.168.0.2", "Local / Office");
    row.host.setComment("first\nsecond");
    model()->setRows({ row });

    EXPECT_EQ(textAt(model(), 0, Column::COMMENT), QString("first second"));

    auto tooltip = [this](Column column)
    {
        return model()->index(0, static_cast<int>(column)).data(Qt::ToolTipRole);
    };

    EXPECT_EQ(tooltip(Column::COMMENT).toString(), QString("first\nsecond"));
    EXPECT_EQ(tooltip(Column::SOURCE).toString(), QString("Local / Office"));
    EXPECT_FALSE(tooltip(Column::NAME).isValid());
}

//--------------------------------------------------------------------------------------------------
// The icon belongs to the column that names the row and is not repeated across it.
TEST_F(SearchResultModelTest, IconIsOnTheNameColumn)
{
    model()->setRows(rows());

    auto hasIcon = [this](int row, Column column)
    {
        return model()->index(row, static_cast<int>(column)).data(Qt::DecorationRole).isValid();
    };

    EXPECT_TRUE(hasIcon(0, Column::NAME));
    EXPECT_FALSE(hasIcon(0, Column::ADDRESS));
    EXPECT_FALSE(hasIcon(2, Column::SOURCE));
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "host2" comes before "host10". The rows of the two kinds are one
// list and are sorted together.
TEST_F(SearchResultModelTest, NameIsSortedTheWayItIsReadAcrossBothKinds)
{
    model()->setRows(rows());

    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);
    EXPECT_EQ(rowNames(model()), QStringList({ "alpha", "host2", "host10" }));

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);
    EXPECT_EQ(rowNames(model()), QStringList({ "host10", "host2", "alpha" }));
}

//--------------------------------------------------------------------------------------------------
// So is an address, so ".2" comes before ".10".
TEST_F(SearchResultModelTest, AddressIsSortedTheWayItIsRead)
{
    model()->setRows(rows());

    model()->sort(static_cast<int>(Column::ADDRESS), Qt::AscendingOrder);

    EXPECT_EQ(rowNames(model()), QStringList({ "alpha", "host2", "host10" }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another host - the
// one a connection would then be made to.
TEST_F(SearchResultModelTest, SortingCarriesTheSelectionToItsRow)
{
    model()->setRows(rows());
    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(2, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->rowAt(selection.currentIndex().row())->host.name(), QString("host10"));

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->rowAt(selection.currentIndex().row())->host.name(), QString("host10"));
}

//--------------------------------------------------------------------------------------------------
// Rows of routers have no id to be told apart by, so the selection is carried by a key of the model
// instead. Found by id they would all be the same row.
TEST_F(SearchResultModelTest, SortingCarriesTheSelectionOfARouterRow)
{
    model()->setRows({ makeRouterRow("beta", "111 111 111", "Router 1"),
                       makeRouterRow("alpha", "222 222 222", "Router 2") });
    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(1, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->rowAt(selection.currentIndex().row())->host.name(), QString("beta"));

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->rowAt(selection.currentIndex().row())->host.name(), QString("beta"));
}

//--------------------------------------------------------------------------------------------------
// The address book is edited by its entry id, and only rows that came from it have one.
TEST_F(SearchResultModelTest, EntryIsFoundAmongLocalRowsOnly)
{
    model()->setRows(rows());

    EXPECT_EQ(model()->rowOfEntry(12), 1);
    EXPECT_EQ(model()->rowOfEntry(777), -1);

    // The id a router row carries, asked for on its own.
    EXPECT_EQ(model()->rowOfEntry(-1), -1);
}

//--------------------------------------------------------------------------------------------------
// The record was edited elsewhere, and the row shows it as it is now. Moving it to another group
// changes the path the row was found under as well.
TEST_F(SearchResultModelTest, UpdatedEntryIsShownAsItIsNow)
{
    model()->setRows(rows());

    HostConfig host;
    host.setId(11);
    host.setName("renamed");
    host.setAddress("10.0.0.1");

    EXPECT_TRUE(model()->updateEntry(host, "Local / Other"));

    EXPECT_EQ(textAt(model(), 0, Column::NAME), QString("renamed"));
    EXPECT_EQ(textAt(model(), 0, Column::ADDRESS), QString("10.0.0.1"));
    EXPECT_EQ(textAt(model(), 0, Column::SOURCE), QString("Local / Other"));
}

//--------------------------------------------------------------------------------------------------
// A record that is not on the page has no row to catch up, and a router row is not a record.
TEST_F(SearchResultModelTest, UpdateOfARecordThatIsNotShownChangesNothing)
{
    model()->setRows(rows());

    HostConfig absent;
    absent.setId(777);
    EXPECT_FALSE(model()->updateEntry(absent, "Local"));

    HostConfig router_host;
    EXPECT_FALSE(model()->updateEntry(router_host, "Router 1"));

    EXPECT_EQ(rowNames(model()), QStringList({ "host2", "host10", "alpha" }));
}

//--------------------------------------------------------------------------------------------------
// The record was deleted from the address book, so its row goes with it.
TEST_F(SearchResultModelTest, RemovedEntryLeavesTheRestOfThePage)
{
    model()->setRows(rows());

    EXPECT_TRUE(model()->removeEntry(11));
    EXPECT_EQ(rowNames(model()), QStringList({ "host10", "alpha" }));

    EXPECT_FALSE(model()->removeEntry(11));
    EXPECT_FALSE(model()->removeEntry(-1));
    EXPECT_EQ(model()->rowCount(), 2);
}

//--------------------------------------------------------------------------------------------------
// Every page of a search is a new list, and it arrives sorted the way the user asked for without
// another click on the header.
TEST_F(SearchResultModelTest, NewPageKeepsTheSortTheUserPicked)
{
    model()->setRows(rows());
    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    model()->setRows(rows());

    EXPECT_EQ(rowNames(model()), QStringList({ "host10", "host2", "alpha" }));
}

//--------------------------------------------------------------------------------------------------
// The query is gone, so what it found is not shown as if it still held.
TEST_F(SearchResultModelTest, ClearDropsTheRows)
{
    model()->setRows(rows());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 4);
}
