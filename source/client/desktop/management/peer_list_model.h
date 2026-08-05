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

#ifndef CLIENT_DESKTOP_MANAGEMENT_PEER_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_PEER_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>

#include "proto/router_admin.h"

// The pairs of peers a single relay is serving. They come with the statistics of that relay, so the
// list is replaced whenever another relay is picked or the statistics arrive again.
class PeerListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        USER_NAME,
        HOST_ID,
        HOST_ADDRESS,
        CLIENT_ADDRESS,
        TRANSFERRED,
        DURATION,
        IDLE
    };

    explicit PeerListModel(QObject* parent = nullptr);
    ~PeerListModel() final;

    void setPeers(const proto::router::RelayInfo::Statistics& statistics);
    void clear();

    // Null when the row is out of range.
    const proto::router::Peer* peerAt(int row) const;

    // Row of the pair with this id, or -1.
    int rowOf(qint64 peer_id) const;

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const proto::router::Peer& peer, Column column) const;
    void applySort();

    QList<proto::router::Peer> peers_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(PeerListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_PEER_LIST_MODEL_H
