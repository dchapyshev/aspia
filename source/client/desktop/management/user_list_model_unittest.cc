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

#include "client/desktop/management/user_list_model.h"

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>

#include <gtest/gtest.h>

#include <memory>

#include "base/peer/user.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

using Column = UserListModel::Column;

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
void addUser(proto::router::UserList* list, qint64 entry_id, const QString& name, quint32 flags,
             quint32 sessions)
{
    proto::router::User* user = list->add_user();
    user->set_entry_id(entry_id);
    user->set_name(name.toStdString());
    user->set_flags(flags);
    user->set_sessions(sessions);
}

} // namespace

// Every model of the fixture is watched by the tester of Qt, which checks the rules an item model
// has to keep and which nothing in the code itself states: that the counts agree with the indexes
// handed out, and that the signals around a change come in the order and the pairing the views
// rely on.
class UserListModelTest : public testing::Test
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

    UserListModel* model()
    {
        if (!model_)
        {
            model_ = std::make_unique<UserListModel>();
            tester_ = std::make_unique<QAbstractItemModelTester>(
                model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
        }

        return model_.get();
    }

    // The three users every test works with: an enabled administrator, a disabled client, and a
    // manager whose name only sorts right when it is read as a person reads it.
    static proto::router::UserList users()
    {
        proto::router::UserList list;

        addUser(&list, 3, "user2", User::ENABLED,
                proto::router::SESSION_TYPE_ADMIN | proto::router::SESSION_TYPE_MANAGER);
        addUser(&list, 1, "user10", 0, proto::router::SESSION_TYPE_OPERATOR);
        addUser(&list, 2, "admin", User::ENABLED, proto::router::SESSION_TYPE_MANAGER);

        return list;
    }

    // Ids of the rows top to bottom, which is what a sort is judged by.
    static QList<qint64> rowIds(const UserListModel* model)
    {
        QList<qint64> ids;
        for (int i = 0; i < model->rowCount(); ++i)
            ids.append(model->userAt(i)->entry_id);
        return ids;
    }

    static QString textAt(const UserListModel* model, int row, Column column)
    {
        return model->index(row, static_cast<int>(column)).data(Qt::DisplayRole).toString();
    }

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    std::unique_ptr<UserListModel> model_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(UserListModelTest, EmptyModelHasItsColumnsAndNoRows)
{
    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 3);
    EXPECT_EQ(model()->userAt(0), nullptr);
    EXPECT_EQ(model()->rowOf(1), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserListModelTest, HeaderNamesTheColumnOfItsSection)
{
    EXPECT_EQ(model()->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Name"));
    EXPECT_EQ(model()->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Enabled"));
    EXPECT_EQ(model()->headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
              QString("Session Types"));

    // A vertical header is not ours to fill, and a section outside the table has no name.
    EXPECT_FALSE(model()->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model()->headerData(9, Qt::Horizontal, Qt::DisplayRole).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserListModelTest, RowShowsTheFieldsOfItsUser)
{
    model()->setUsers(users());

    ASSERT_EQ(model()->rowCount(), 3);

    EXPECT_EQ(textAt(model(), 0, Column::NAME), QString("user2"));
    EXPECT_EQ(textAt(model(), 0, Column::ENABLED), QString("Yes"));
    EXPECT_EQ(textAt(model(), 1, Column::ENABLED), QString("No"));
}

//--------------------------------------------------------------------------------------------------
// An account can hold more than one kind of session, and the column names all of them.
TEST_F(UserListModelTest, SessionTypesAreListed)
{
    model()->setUsers(users());

    EXPECT_EQ(textAt(model(), 0, Column::SESSIONS), QString("Administrator, Manager"));
    EXPECT_EQ(textAt(model(), 1, Column::SESSIONS), QString("Operator"));
}

//--------------------------------------------------------------------------------------------------
// An account with no kind of session at all is possible, and the column then says nothing rather
// than something wrong.
TEST_F(UserListModelTest, UserWithoutSessionTypesShowsNothing)
{
    proto::router::UserList list;
    addUser(&list, 1, "nobody", User::ENABLED, 0);

    model()->setUsers(list);

    EXPECT_TRUE(textAt(model(), 0, Column::SESSIONS).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A disabled account is told apart by its icon as well, and the icon belongs to the column that
// names the user.
TEST_F(UserListModelTest, IconIsOnTheNameColumn)
{
    model()->setUsers(users());

    auto hasIcon = [this](int row, Column column)
    {
        return model()->index(row, static_cast<int>(column)).data(Qt::DecorationRole).isValid();
    };

    EXPECT_TRUE(hasIcon(0, Column::NAME));
    EXPECT_FALSE(hasIcon(0, Column::ENABLED));
    EXPECT_FALSE(hasIcon(0, Column::SESSIONS));
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserListModelTest, EnabledAndDisabledAccountsLookDifferent)
{
    model()->setUsers(users());

    const QVariant enabled = model()->index(0, static_cast<int>(Column::NAME)).data(Qt::DecorationRole);
    const QVariant disabled = model()->index(1, static_cast<int>(Column::NAME)).data(Qt::DecorationRole);

    ASSERT_TRUE(enabled.isValid());
    ASSERT_TRUE(disabled.isValid());
    EXPECT_NE(enabled.value<QIcon>().cacheKey(), disabled.value<QIcon>().cacheKey());
}

//--------------------------------------------------------------------------------------------------
// A name is read by a person, so "user2" comes before "user10".
TEST_F(UserListModelTest, NameIsSortedTheWayItIsRead)
{
    model()->setUsers(users());

    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 2, 3, 1 }));

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 3, 2 }));
}

//--------------------------------------------------------------------------------------------------
// A column with nothing special about it is sorted by what it shows, and records that read the same
// keep the order they came in.
TEST_F(UserListModelTest, PlainColumnIsSortedByItsTextAndKeepsTheRestInPlace)
{
    model()->setUsers(users());

    model()->sort(static_cast<int>(Column::ENABLED), Qt::AscendingOrder);

    // "No" before "Yes", and the two enabled ones stay in the order the router listed them.
    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 3, 2 }));
}

//--------------------------------------------------------------------------------------------------
// The list is refetched on every change, and it arrives sorted the way the user asked for without
// another click on the header.
TEST_F(UserListModelTest, NewListKeepsTheSortTheUserPicked)
{
    model()->setUsers(users());
    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    model()->setUsers(users());

    EXPECT_EQ(rowIds(model()), QList<qint64>({ 1, 3, 2 }));
}

//--------------------------------------------------------------------------------------------------
// A selection is a persistent index, and sorting moves the rows under it. Unless the model carries
// the index over, the view keeps the selection on the row number and it lands on another account -
// the one a delete would then act on.
TEST_F(UserListModelTest, SortingCarriesTheSelectionToItsUser)
{
    model()->setUsers(users());
    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    QItemSelectionModel selection(model());
    selection.setCurrentIndex(model()->index(0, 0), QItemSelectionModel::SelectCurrent);

    ASSERT_EQ(model()->userAt(selection.currentIndex().row())->entry_id, 2);

    model()->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);

    ASSERT_TRUE(selection.currentIndex().isValid());
    EXPECT_EQ(model()->userAt(selection.currentIndex().row())->entry_id, 2);
}

//--------------------------------------------------------------------------------------------------
// The list is replaced whole on every refetch, so the row a user sits on is found by its id.
TEST_F(UserListModelTest, UserIsFoundByItsId)
{
    model()->setUsers(users());
    model()->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    EXPECT_EQ(model()->rowOf(1), 2);
    EXPECT_EQ(model()->userAt(2)->entry_id, 1);
    EXPECT_EQ(model()->rowOf(777), -1);
}

//--------------------------------------------------------------------------------------------------
// The session of the router is gone, so what it answered is not shown as if it still held.
TEST_F(UserListModelTest, ClearDropsTheRows)
{
    model()->setUsers(users());

    model()->clear();

    EXPECT_EQ(model()->rowCount(), 0);
    EXPECT_EQ(model()->columnCount(), 3);
}
