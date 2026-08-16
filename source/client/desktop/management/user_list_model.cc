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

#include <QIcon>
#include <QStringList>

#include <algorithm>

#include "base/peer/user.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

constexpr int kColumnCount = 3;

} // namespace

//--------------------------------------------------------------------------------------------------
UserListModel::UserListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
UserListModel::~UserListModel() = default;

//--------------------------------------------------------------------------------------------------
void UserListModel::setUsers(const proto::router::UserList& list)
{
    beginResetModel();

    users_.clear();
    users_.reserve(list.user_size());

    for (int i = 0; i < list.user_size(); ++i)
        users_.append(RouterUser::parseFrom(list.user(i)));

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void UserListModel::clear()
{
    beginResetModel();
    users_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const RouterUser* UserListModel::userAt(int row) const
{
    if (row < 0 || row >= users_.size())
        return nullptr;

    return &users_[row];
}

//--------------------------------------------------------------------------------------------------
int UserListModel::rowOf(qint64 entry_id) const
{
    for (int i = 0; i < users_.size(); ++i)
    {
        if (users_[i].entry_id == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int UserListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(users_.size());
}

//--------------------------------------------------------------------------------------------------
int UserListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    const RouterUser* user = userAt(index.row());
    if (!user || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        // A disabled account is told apart by its icon, not only by the column that says so.
        if (column != Column::NAME)
            return QVariant();

        return QIcon((user->flags & User::ENABLED) ? ":/img/user.svg" : ":/img/locked-user.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*user, column);
}

//--------------------------------------------------------------------------------------------------
QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::NAME:
            return tr("Name");

        case Column::ENABLED:
            return tr("Enabled");

        case Column::SESSIONS:
            return tr("Session Types");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void UserListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another user.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const RouterUser* user = userAt(old_index.row());
        selected_ids.append(user ? user->entry_id : 0);
    }

    applySort();

    QModelIndexList new_indexes;
    new_indexes.reserve(old_indexes.size());

    for (int i = 0; i < old_indexes.size(); ++i)
    {
        const int row = rowOf(selected_ids[i]);
        new_indexes.append(row < 0 ? QModelIndex() : index(row, old_indexes[i].column()));
    }

    changePersistentIndexList(old_indexes, new_indexes);

    emit layoutChanged();
}

//--------------------------------------------------------------------------------------------------
QString UserListModel::textAt(const RouterUser& user, Column column) const
{
    switch (column)
    {
        case Column::NAME:
            return user.name;

        case Column::ENABLED:
            return (user.flags & User::ENABLED) ? tr("Yes") : tr("No");

        case Column::SESSIONS:
        {
            QStringList sessions;

            if (user.sessions & proto::router::SESSION_TYPE_ADMIN)
                sessions.append(tr("Administrator"));
            if (user.sessions & proto::router::SESSION_TYPE_MANAGER)
                sessions.append(tr("Manager"));
            if (user.sessions & proto::router::SESSION_TYPE_OPERATOR)
                sessions.append(tr("Operator"));

            return sessions.join(", ");
        }
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void UserListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // A name is compared the way the user reads it, so "user2" comes before "user10".
    auto less = [&](const RouterUser& first, const RouterUser& second)
    {
        if (column == Column::NAME)
            return collator_.compare(first.name, second.name) < 0;

        return textAt(first, column) < textAt(second, column);
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(users_.begin(), users_.end(),
                     [&](const RouterUser& first, const RouterUser& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
