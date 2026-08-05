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

#include "client/desktop/management/relay_list_model.h"

#include <QDateTime>
#include <QIcon>
#include <QLocale>

#include <algorithm>

#include "proto/peer.h"

namespace {

constexpr int kColumnCount = 7;

} // namespace

//--------------------------------------------------------------------------------------------------
RelayListModel::RelayListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
RelayListModel::~RelayListModel() = default;

//--------------------------------------------------------------------------------------------------
void RelayListModel::setRelays(const proto::router::RelayList& list)
{
    beginResetModel();

    relays_.clear();
    relays_.reserve(list.relay_size());

    for (int i = 0; i < list.relay_size(); ++i)
        relays_.append(list.relay(i));

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void RelayListModel::clear()
{
    beginResetModel();
    relays_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const proto::router::RelayInfo* RelayListModel::relayAt(int row) const
{
    if (row < 0 || row >= relays_.size())
        return nullptr;

    return &relays_[row];
}

//--------------------------------------------------------------------------------------------------
int RelayListModel::rowOf(qint64 entry_id) const
{
    for (int i = 0; i < relays_.size(); ++i)
    {
        if (relays_[i].entry_id() == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int RelayListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(relays_.size());
}

//--------------------------------------------------------------------------------------------------
int RelayListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant RelayListModel::data(const QModelIndex& index, int role) const
{
    const proto::router::RelayInfo* relay = relayAt(index.row());
    if (!relay || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::ADDRESS)
            return QVariant();

        return QIcon(":/img/stack.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*relay, column);
}

//--------------------------------------------------------------------------------------------------
QVariant RelayListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::ADDRESS:
            return tr("Address");

        case Column::CONNECT_TIME:
            return tr("Connect Time");

        case Column::POOL_SIZE:
            return tr("Pool Size");

        case Column::VERSION:
            return tr("Version");

        case Column::COMPUTER_NAME:
            return tr("Computer Name");

        case Column::ARCHITECTURE:
            return tr("Architecture");

        case Column::OS:
            return tr("Operating System");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void RelayListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another relay.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const proto::router::RelayInfo* relay = relayAt(old_index.row());
        selected_ids.append(relay ? relay->entry_id() : 0);
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
QString RelayListModel::textAt(const proto::router::RelayInfo& relay, Column column) const
{
    switch (column)
    {
        case Column::ADDRESS:
            return QString::fromStdString(relay.ip_address());

        case Column::CONNECT_TIME:
            return QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(relay.timepoint()), QLocale::ShortFormat);

        case Column::POOL_SIZE:
            return QString::number(relay.pool_size());

        case Column::VERSION:
        {
            const proto::peer::Version& version = relay.version();
            return QString("%1.%2.%3")
                .arg(version.major()).arg(version.minor()).arg(version.patch());
        }

        case Column::COMPUTER_NAME:
            return QString::fromStdString(relay.computer_name());

        case Column::ARCHITECTURE:
            return QString::fromStdString(relay.architecture());

        case Column::OS:
            return QString::fromStdString(relay.os_name());
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void RelayListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // The moment of the connection and the size of the pool are compared as the numbers they are
    // and not as the text they are drawn as, or a pool of 10 keys would come before one of 9. An
    // address is compared the way the user reads it, so ".2" comes before ".10".
    auto less = [&](const proto::router::RelayInfo& first, const proto::router::RelayInfo& second)
    {
        if (column == Column::CONNECT_TIME)
            return first.timepoint() < second.timepoint();

        if (column == Column::POOL_SIZE)
            return first.pool_size() < second.pool_size();

        if (column == Column::ADDRESS)
        {
            return collator_.compare(QString::fromStdString(first.ip_address()),
                                     QString::fromStdString(second.ip_address())) < 0;
        }

        return textAt(first, column) < textAt(second, column);
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(relays_.begin(), relays_.end(),
                     [&](const proto::router::RelayInfo& first,
                         const proto::router::RelayInfo& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
