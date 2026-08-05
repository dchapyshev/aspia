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

#include "client/desktop/management/client_list_model.h"

#include <QDateTime>
#include <QIcon>
#include <QLocale>

#include <algorithm>

#include "proto/peer.h"

namespace {

constexpr int kColumnCount = 6;

} // namespace

//--------------------------------------------------------------------------------------------------
ClientListModel::ClientListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
ClientListModel::~ClientListModel() = default;

//--------------------------------------------------------------------------------------------------
void ClientListModel::setClients(const proto::router::ClientList& list)
{
    beginResetModel();

    clients_.clear();
    clients_.reserve(list.client_size());

    for (int i = 0; i < list.client_size(); ++i)
        clients_.append(list.client(i));

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void ClientListModel::clear()
{
    beginResetModel();
    clients_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const proto::router::ClientInfo* ClientListModel::clientAt(int row) const
{
    if (row < 0 || row >= clients_.size())
        return nullptr;

    return &clients_[row];
}

//--------------------------------------------------------------------------------------------------
int ClientListModel::rowOf(qint64 entry_id) const
{
    for (int i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i].entry_id() == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int ClientListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(clients_.size());
}

//--------------------------------------------------------------------------------------------------
int ClientListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant ClientListModel::data(const QModelIndex& index, int role) const
{
    const proto::router::ClientInfo* client = clientAt(index.row());
    if (!client || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::COMPUTER_NAME)
            return QVariant();

        return QIcon(":/img/computer.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*client, column);
}

//--------------------------------------------------------------------------------------------------
QVariant ClientListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::COMPUTER_NAME:
            return tr("Computer Name");

        case Column::IP_ADDRESS:
            return tr("IP Address");

        case Column::CONNECT_TIME:
            return tr("Connect Time");

        case Column::VERSION:
            return tr("Version");

        case Column::ARCHITECTURE:
            return tr("Architecture");

        case Column::OS:
            return tr("Operating System");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void ClientListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another session.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const proto::router::ClientInfo* client = clientAt(old_index.row());
        selected_ids.append(client ? client->entry_id() : 0);
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
QString ClientListModel::textAt(const proto::router::ClientInfo& client, Column column) const
{
    switch (column)
    {
        case Column::COMPUTER_NAME:
            return QString::fromStdString(client.computer_name());

        case Column::IP_ADDRESS:
            return QString::fromStdString(client.ip_address());

        case Column::CONNECT_TIME:
            return QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(client.timepoint()), QLocale::ShortFormat);

        case Column::VERSION:
        {
            const proto::peer::Version& version = client.version();
            return QString("%1.%2.%3")
                .arg(version.major()).arg(version.minor()).arg(version.patch());
        }

        case Column::ARCHITECTURE:
            return QString::fromStdString(client.architecture());

        case Column::OS:
            return QString::fromStdString(client.os_name());
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void ClientListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // The moment of the connection is compared as the time it is and not as the text it is drawn
    // as, which would sort by the order the parts of a date happen to be written in. A name is
    // compared the way the user reads it, so "host2" comes before "host10".
    auto less = [&](const proto::router::ClientInfo& first,
                    const proto::router::ClientInfo& second)
    {
        if (column == Column::CONNECT_TIME)
            return first.timepoint() < second.timepoint();

        if (column == Column::COMPUTER_NAME)
        {
            return collator_.compare(QString::fromStdString(first.computer_name()),
                                     QString::fromStdString(second.computer_name())) < 0;
        }

        return textAt(first, column) < textAt(second, column);
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(clients_.begin(), clients_.end(),
                     [&](const proto::router::ClientInfo& first,
                         const proto::router::ClientInfo& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
