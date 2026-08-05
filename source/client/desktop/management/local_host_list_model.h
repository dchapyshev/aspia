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

#ifndef CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>
#include <QHash>

#include "client/config.h"

// The hosts of one group of the address book. They come from the local database, so unlike the
// lists of a router they are edited a record at a time rather than replaced whole.
//
// Whether a host answers is not part of its record: it is probed after the group is shown and holds
// only until the group changes.
class LocalHostListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        NAME,
        ADDRESS,
        COMMENT,
        CREATED,
        MODIFIED,
        CONNECT,
        STATUS
    };

    explicit LocalHostListModel(QObject* parent = nullptr);
    ~LocalHostListModel() final;

    // Shows another group. The probed states of the previous one go with it.
    void setHosts(const QList<HostConfig>& hosts);
    void clear();

    // Null when the row is out of range.
    const HostConfig* hostAt(int row) const;
    int rowOf(qint64 entry_id) const;

    // The records themselves, for whoever needs to work with them rather than show them.
    const QList<HostConfig>& hosts() const { return hosts_; }

    // The record was edited elsewhere and its row has to catch up. False when it is not shown here.
    bool updateHost(const HostConfig& host);
    bool removeHost(qint64 entry_id);

    // A connection was just made to the host, which is the only field of a record this list writes.
    void setConnectTime(qint64 entry_id, qint64 connect_time);

    // The answer of a probe. Until one arrives a host is neither online nor offline, and the row
    // says nothing about it.
    void setOnlineStatus(qint64 entry_id, bool online);
    void clearOnlineStatuses();

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const HostConfig& host, Column column) const;
    void applySort();
    void emitRowChanged(int row);

    QList<HostConfig> hosts_;

    // Probed states by entry id. A host that is not in it has not been probed.
    QHash<qint64, bool> online_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(LocalHostListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_LIST_MODEL_H
