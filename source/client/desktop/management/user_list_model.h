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

#ifndef CLIENT_DESKTOP_MANAGEMENT_USER_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_USER_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>

#include "base/peer/router_user.h"

namespace proto::router {
class UserList;
} // namespace proto::router

// The users of a router. The whole list arrives at once, so unlike the hosts there is no page to
// keep track of.
class UserListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        NAME,
        ENABLED,
        SESSIONS
    };

    explicit UserListModel(QObject* parent = nullptr);
    ~UserListModel() final;

    // Replaces the list. The sort the user picked is applied to it.
    void setUsers(const proto::router::UserList& list);
    void clear();

    // Null when the row is out of range.
    const RouterUser* userAt(int row) const;

    // Row of the user with this id, or -1. Used to keep the selection across a refetch, which
    // arrives as a whole list and not as an edit of the old one.
    int rowOf(qint64 entry_id) const;

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const RouterUser& user, Column column) const;
    void applySort();

    QList<RouterUser> users_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(UserListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_USER_LIST_MODEL_H
