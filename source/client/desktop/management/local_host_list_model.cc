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

#include "client/desktop/management/local_host_list_model.h"

#include <QDateTime>
#include <QIcon>
#include <QLocale>

#include <algorithm>

namespace {

constexpr int kColumnCount = 7;

//--------------------------------------------------------------------------------------------------
QString timestampToString(qint64 timestamp)
{
    if (timestamp <= 0)
        return QString();

    return QLocale::system().toString(
        QDateTime::fromSecsSinceEpoch(timestamp), QLocale::ShortFormat);
}

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostListModel::LocalHostListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
LocalHostListModel::~LocalHostListModel() = default;

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::setHosts(const QList<LocalHostConfig>& hosts)
{
    beginResetModel();
    hosts_ = hosts;
    online_.clear();
    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::clear()
{
    beginResetModel();
    hosts_.clear();
    online_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const LocalHostConfig* LocalHostListModel::hostAt(int row) const
{
    if (row < 0 || row >= hosts_.size())
        return nullptr;

    return &hosts_[row];
}

//--------------------------------------------------------------------------------------------------
int LocalHostListModel::rowOf(qint64 entry_id) const
{
    for (int i = 0; i < hosts_.size(); ++i)
    {
        if (hosts_[i].id() == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostListModel::updateHost(const LocalHostConfig& host)
{
    const int row = rowOf(host.id());
    if (row < 0)
        return false;

    hosts_[row] = host;
    emitRowChanged(row);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostListModel::removeHost(qint64 entry_id)
{
    const int row = rowOf(entry_id);
    if (row < 0)
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    hosts_.removeAt(row);
    online_.remove(entry_id);
    endRemoveRows();

    return true;
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::setConnectTime(qint64 entry_id, qint64 connect_time)
{
    const int row = rowOf(entry_id);
    if (row < 0)
        return;

    hosts_[row].setConnectTime(connect_time);
    emitRowChanged(row);
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::setOnlineStatus(qint64 entry_id, bool online)
{
    const int row = rowOf(entry_id);
    if (row < 0)
        return;

    online_.insert(entry_id, online);
    emitRowChanged(row);
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::clearOnlineStatuses()
{
    if (online_.isEmpty())
        return;

    online_.clear();

    const int last_row = rowCount() - 1;
    if (last_row < 0)
        return;

    emit dataChanged(index(0, 0), index(last_row, kColumnCount - 1),
                     { Qt::DisplayRole, Qt::DecorationRole });
}

//--------------------------------------------------------------------------------------------------
int LocalHostListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(hosts_.size());
}

//--------------------------------------------------------------------------------------------------
int LocalHostListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant LocalHostListModel::data(const QModelIndex& index, int role) const
{
    const LocalHostConfig* host = hostAt(index.row());
    if (!host || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::NAME)
            return QVariant();

        const auto it = online_.constFind(host->id());
        if (it == online_.constEnd())
            return QIcon(":/img/computer.svg");

        return QIcon(*it ? ":/img/computer-online.svg" : ":/img/computer-offline.svg");
    }

    // The comment is drawn on one line, so the whole of it is only readable in the tooltip.
    if (role == Qt::ToolTipRole)
        return column == Column::COMMENT ? host->comment() : QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*host, column);
}

//--------------------------------------------------------------------------------------------------
QVariant LocalHostListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::NAME:
            return tr("Name");

        case Column::ADDRESS:
            return tr("Address / ID");

        case Column::COMMENT:
            return tr("Comment");

        case Column::CREATED:
            return tr("Created");

        case Column::MODIFIED:
            return tr("Modified");

        case Column::CONNECT:
            return tr("Last Connect");

        case Column::STATUS:
            return tr("Status");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another host.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const LocalHostConfig* host = hostAt(old_index.row());
        selected_ids.append(host ? host->id() : 0);
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
QString LocalHostListModel::textAt(const LocalHostConfig& host, Column column) const
{
    switch (column)
    {
        case Column::NAME:
            return host.name();

        case Column::ADDRESS:
            return host.address();

        case Column::COMMENT:
        {
            // One row, one line: a comment of several lines would otherwise stretch every row of
            // the list to its height.
            QString comment = host.comment();
            return comment.replace('\n', ' ').replace('\r', ' ');
        }

        case Column::CREATED:
            return timestampToString(host.createTime());

        case Column::MODIFIED:
            return timestampToString(host.modifyTime());

        case Column::CONNECT:
            return timestampToString(host.connectTime());

        case Column::STATUS:
        {
            const auto it = online_.constFind(host.id());
            if (it == online_.constEnd())
                return QString();

            return *it ? tr("Online") : tr("Offline");
        }
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // The moments in time are compared as the times they are and not as the text they are drawn as,
    // which would sort by the order the parts of a date happen to be written in. A name and an
    // address are compared the way the user reads them, so "host2" comes before "host10".
    auto less = [&](const LocalHostConfig& first, const LocalHostConfig& second)
    {
        switch (column)
        {
            case Column::CREATED:
                return first.createTime() < second.createTime();

            case Column::MODIFIED:
                return first.modifyTime() < second.modifyTime();

            case Column::CONNECT:
                return first.connectTime() < second.connectTime();

            case Column::NAME:
            case Column::ADDRESS:
                return collator_.compare(textAt(first, column), textAt(second, column)) < 0;

            default:
                return textAt(first, column) < textAt(second, column);
        }
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(hosts_.begin(), hosts_.end(),
                     [&](const LocalHostConfig& first, const LocalHostConfig& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}

//--------------------------------------------------------------------------------------------------
void LocalHostListModel::emitRowChanged(int row)
{
    emit dataChanged(index(row, 0), index(row, kColumnCount - 1),
                     { Qt::DisplayRole, Qt::DecorationRole, Qt::ToolTipRole });
}
