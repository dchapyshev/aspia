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

#ifndef CLIENT_DESKTOP_MANAGEMENT_CLIENT_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_CLIENT_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>

#include "proto/router_admin.h"

// The sessions a router is serving right now. Nothing about them is stored anywhere, so the records
// are kept as the router sent them.
class ClientListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        COMPUTER_NAME,
        IP_ADDRESS,
        CONNECT_TIME,
        VERSION,
        ARCHITECTURE,
        OS
    };

    explicit ClientListModel(QObject* parent = nullptr);
    ~ClientListModel() final;

    // Replaces the list. The sort the user picked is applied to it.
    void setClients(const proto::router::ClientList& list);
    void clear();

    // Null when the row is out of range.
    const proto::router::ClientInfo* clientAt(int row) const;

    // Row of the session with this id, or -1. Used to keep the selection across a refetch, which
    // arrives as a whole list and not as an edit of the old one.
    int rowOf(qint64 entry_id) const;

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const proto::router::ClientInfo& client, Column column) const;
    void applySort();

    QList<proto::router::ClientInfo> clients_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(ClientListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_CLIENT_LIST_MODEL_H
