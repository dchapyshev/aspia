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

#ifndef CLIENT_DESKTOP_MANAGEMENT_TEMP_HOST_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_TEMP_HOST_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>

#include "client/router_types.h"

// The hosts that connected to a router but were not approved by an administrator yet. They live
// only as long as their session does, so nothing about them is stored anywhere.
class TempHostListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        ID,
        COMPUTER_NAME,
        OS,
        VERSION,
        ADDRESS
    };

    explicit TempHostListModel(QObject* parent = nullptr);
    ~TempHostListModel() final;

    // Replaces the list. The sort the user picked is applied to it.
    void setHosts(const QList<RouterTempHost>& hosts);
    void clear();

    // Null when the row is out of range.
    const RouterTempHost* hostAt(int row) const;

    // Row of the host with this id, or -1. Used to keep the selection across a refetch, which
    // arrives as a whole list and not as an edit of the old one.
    int rowOf(HostId temp_id) const;

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const RouterTempHost& host, Column column) const;
    void applySort();

    QList<RouterTempHost> hosts_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(TempHostListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_TEMP_HOST_LIST_MODEL_H
