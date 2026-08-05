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

#include "client/desktop/management/temp_host_list_model.h"

#include <QIcon>

#include <algorithm>

namespace {

constexpr int kColumnCount = 5;

} // namespace

//--------------------------------------------------------------------------------------------------
TempHostListModel::TempHostListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
TempHostListModel::~TempHostListModel() = default;

//--------------------------------------------------------------------------------------------------
void TempHostListModel::setHosts(const QList<RouterTempHost>& hosts)
{
    beginResetModel();
    hosts_ = hosts;
    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void TempHostListModel::clear()
{
    beginResetModel();
    hosts_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const RouterTempHost* TempHostListModel::hostAt(int row) const
{
    if (row < 0 || row >= hosts_.size())
        return nullptr;

    return &hosts_[row];
}

//--------------------------------------------------------------------------------------------------
int TempHostListModel::rowOf(HostId temp_id) const
{
    for (int i = 0; i < hosts_.size(); ++i)
    {
        if (hosts_[i].temp_id == temp_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int TempHostListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(hosts_.size());
}

//--------------------------------------------------------------------------------------------------
int TempHostListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant TempHostListModel::data(const QModelIndex& index, int role) const
{
    const RouterTempHost* host = hostAt(index.row());
    if (!host || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::ID)
            return QVariant();

        return QIcon(":/img/computer.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*host, column);
}

//--------------------------------------------------------------------------------------------------
QVariant TempHostListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::ID:
            return tr("ID");

        case Column::COMPUTER_NAME:
            return tr("Computer Name");

        case Column::OS:
            return tr("Operating System");

        case Column::VERSION:
            return tr("Version");

        case Column::ADDRESS:
            return tr("Address");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void TempHostListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another host.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<HostId> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const RouterTempHost* host = hostAt(old_index.row());
        selected_ids.append(host ? host->temp_id : kInvalidHostId);
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
QString TempHostListModel::textAt(const RouterTempHost& host, Column column) const
{
    switch (column)
    {
        case Column::ID:
            return QString::number(host.temp_id);

        case Column::COMPUTER_NAME:
            return host.computer_name;

        case Column::OS:
            return host.os_name;

        case Column::VERSION:
            return host.version;

        case Column::ADDRESS:
            return host.address;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void TempHostListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // The id is compared as the number it is. A name and an address are compared the way the user
    // reads them, so "host2" comes before "host10".
    auto less = [&](const RouterTempHost& first, const RouterTempHost& second)
    {
        if (column == Column::ID)
            return first.temp_id < second.temp_id;

        if (column == Column::COMPUTER_NAME || column == Column::ADDRESS)
            return collator_.compare(textAt(first, column), textAt(second, column)) < 0;

        return textAt(first, column) < textAt(second, column);
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(hosts_.begin(), hosts_.end(),
                     [&](const RouterTempHost& first, const RouterTempHost& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
