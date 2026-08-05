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

#include "client/desktop/management/peer_list_model.h"

#include <QIcon>

#include <algorithm>

#include "base/time_types.h"
#include "common/desktop/formatter.h"

namespace {

constexpr int kColumnCount = 7;

} // namespace

//--------------------------------------------------------------------------------------------------
PeerListModel::PeerListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
PeerListModel::~PeerListModel() = default;

//--------------------------------------------------------------------------------------------------
void PeerListModel::setPeers(const proto::router::RelayInfo::Statistics& statistics)
{
    beginResetModel();

    peers_.clear();
    peers_.reserve(statistics.peer_size());

    for (int i = 0; i < statistics.peer_size(); ++i)
        peers_.append(statistics.peer(i));

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void PeerListModel::clear()
{
    beginResetModel();
    peers_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const proto::router::Peer* PeerListModel::peerAt(int row) const
{
    if (row < 0 || row >= peers_.size())
        return nullptr;

    return &peers_[row];
}

//--------------------------------------------------------------------------------------------------
int PeerListModel::rowOf(qint64 peer_id) const
{
    for (int i = 0; i < peers_.size(); ++i)
    {
        if (peers_[i].peer_id() == peer_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int PeerListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(peers_.size());
}

//--------------------------------------------------------------------------------------------------
int PeerListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant PeerListModel::data(const QModelIndex& index, int role) const
{
    const proto::router::Peer* peer = peerAt(index.row());
    if (!peer || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::USER_NAME)
            return QVariant();

        return QIcon(":/img/user.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*peer, column);
}

//--------------------------------------------------------------------------------------------------
QVariant PeerListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::USER_NAME:
            return tr("User Name");

        case Column::HOST_ID:
            return tr("Host ID");

        case Column::HOST_ADDRESS:
            return tr("Host Address");

        case Column::CLIENT_ADDRESS:
            return tr("Client Address");

        case Column::TRANSFERRED:
            return tr("Transferred");

        case Column::DURATION:
            return tr("Duration");

        case Column::IDLE:
            return tr("Idle");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void PeerListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another pair.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const proto::router::Peer* peer = peerAt(old_index.row());
        selected_ids.append(peer ? peer->peer_id() : 0);
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
QString PeerListModel::textAt(const proto::router::Peer& peer, Column column) const
{
    switch (column)
    {
        case Column::USER_NAME:
            return QString::fromStdString(peer.client_user_name());

        case Column::HOST_ID:
            return QString::number(peer.host_id());

        case Column::HOST_ADDRESS:
            return QString::fromStdString(peer.host_address());

        case Column::CLIENT_ADDRESS:
            return QString::fromStdString(peer.client_address());

        case Column::TRANSFERRED:
            return Formatter::sizeToString(peer.bytes_transferred());

        case Column::DURATION:
            return Formatter::delayToString(Seconds(peer.duration()));

        case Column::IDLE:
            return Formatter::delayToString(Seconds(peer.idle_time()));
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void PeerListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // Everything that is drawn as a rounded amount of bytes or of time is compared by the value it
    // was rounded from, or "1 GB" would come before "2 MB". A name is compared the way the user
    // reads it.
    auto less = [&](const proto::router::Peer& first, const proto::router::Peer& second)
    {
        switch (column)
        {
            case Column::HOST_ID:
                return first.host_id() < second.host_id();

            case Column::TRANSFERRED:
                return first.bytes_transferred() < second.bytes_transferred();

            case Column::DURATION:
                return first.duration() < second.duration();

            case Column::IDLE:
                return first.idle_time() < second.idle_time();

            case Column::USER_NAME:
                return collator_.compare(QString::fromStdString(first.client_user_name()),
                                         QString::fromStdString(second.client_user_name())) < 0;

            default:
                return textAt(first, column) < textAt(second, column);
        }
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(peers_.begin(), peers_.end(),
                     [&](const proto::router::Peer& first, const proto::router::Peer& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
