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

#include "client/desktop/management/search_result_model.h"

#include <QIcon>

#include <algorithm>

namespace {

constexpr int kColumnCount = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
SearchResultModel::~SearchResultModel() = default;

//--------------------------------------------------------------------------------------------------
void SearchResultModel::setRows(const QList<Row>& rows)
{
    beginResetModel();

    entries_.clear();
    entries_.reserve(rows.size());

    for (const Row& row : rows)
        entries_.append({ row, next_key_++ });

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void SearchResultModel::clear()
{
    beginResetModel();
    entries_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const SearchResultModel::Row* SearchResultModel::rowAt(int row) const
{
    if (row < 0 || row >= entries_.size())
        return nullptr;

    return &entries_[row].row;
}

//--------------------------------------------------------------------------------------------------
int SearchResultModel::rowOfEntry(qint64 entry_id) const
{
    // A router row has no record and keeps the id a record never has, so it is never the answer.
    if (entry_id < 0)
        return -1;

    for (int i = 0; i < entries_.size(); ++i)
    {
        const Entry& entry = entries_[i];
        if (entry.row.type == Type::LOCAL && entry.row.host.id() == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
bool SearchResultModel::updateEntry(const HostConfig& host, const QString& source)
{
    const int row = rowOfEntry(host.id());
    if (row < 0)
        return false;

    entries_[row].row.host = host;
    entries_[row].row.source = source;

    emit dataChanged(index(row, 0), index(row, kColumnCount - 1),
                     { Qt::DisplayRole, Qt::ToolTipRole });
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SearchResultModel::removeEntry(qint64 entry_id)
{
    const int row = rowOfEntry(entry_id);
    if (row < 0)
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    entries_.removeAt(row);
    endRemoveRows();

    return true;
}

//--------------------------------------------------------------------------------------------------
int SearchResultModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(entries_.size());
}

//--------------------------------------------------------------------------------------------------
int SearchResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant SearchResultModel::data(const QModelIndex& index, int role) const
{
    const Row* row = rowAt(index.row());
    if (!row || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::NAME)
            return QVariant();

        return QIcon(":/img/computer.svg");
    }

    // Both are drawn on one line, so the whole of them is only readable in the tooltip.
    if (role == Qt::ToolTipRole)
    {
        if (column == Column::SOURCE)
            return row->source;

        return column == Column::COMMENT ? row->host.comment() : QVariant();
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*row, column);
}

//--------------------------------------------------------------------------------------------------
QVariant SearchResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::NAME:
            return tr("Name");

        case Column::ADDRESS:
            return tr("Address / ID");

        case Column::SOURCE:
            return tr("Group");

        case Column::COMMENT:
            return tr("Comment");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void SearchResultModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another host.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<quint64> selected_keys;
    selected_keys.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const int row = old_index.row();
        selected_keys.append(row >= 0 && row < entries_.size() ? entries_[row].key : 0);
    }

    applySort();

    QModelIndexList new_indexes;
    new_indexes.reserve(old_indexes.size());

    for (int i = 0; i < old_indexes.size(); ++i)
    {
        const int row = rowOfKey(selected_keys[i]);
        new_indexes.append(row < 0 ? QModelIndex() : index(row, old_indexes[i].column()));
    }

    changePersistentIndexList(old_indexes, new_indexes);

    emit layoutChanged();
}

//--------------------------------------------------------------------------------------------------
QString SearchResultModel::textAt(const Row& row, Column column) const
{
    switch (column)
    {
        case Column::NAME:
            return row.host.name();

        case Column::ADDRESS:
            return row.host.address();

        case Column::SOURCE:
            return row.source;

        case Column::COMMENT:
        {
            // One row, one line: a comment of several lines would otherwise stretch every row of
            // the list to its height.
            QString comment = row.host.comment();
            return comment.replace('\n', ' ').replace('\r', ' ');
        }
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void SearchResultModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    // Every column is text a person reads, so "host2" comes before "host10" in all of them.
    const Column column = static_cast<Column>(sort_column_);
    const bool ascending = sort_order_ == Qt::AscendingOrder;

    auto less = [&](const Entry& first, const Entry& second)
    {
        return collator_.compare(textAt(first.row, column), textAt(second.row, column)) < 0;
    };

    std::stable_sort(entries_.begin(), entries_.end(),
                     [&](const Entry& first, const Entry& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}

//--------------------------------------------------------------------------------------------------
int SearchResultModel::rowOfKey(quint64 key) const
{
    for (int i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].key == key)
            return i;
    }

    return -1;
}
