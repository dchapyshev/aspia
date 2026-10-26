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

#include "client/desktop/management/access_log_list_model.h"

#include <QDateTime>
#include <QIcon>
#include <QLocale>

#include <algorithm>

#include "base/peer/host_id.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"

namespace {

constexpr int kColumnCount = 10;

} // namespace

//--------------------------------------------------------------------------------------------------
AccessLogListModel::AccessLogListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
AccessLogListModel::~AccessLogListModel() = default;

//--------------------------------------------------------------------------------------------------
void AccessLogListModel::setEntries(const proto::router::AccessLogList& list)
{
    beginResetModel();

    entries_.clear();
    entries_.reserve(list.entry_size());

    for (int i = 0; i < list.entry_size(); ++i)
        entries_.append(list.entry(i));

    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void AccessLogListModel::clear()
{
    beginResetModel();
    entries_.clear();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const proto::router::AccessLogEntry* AccessLogListModel::entryAt(int row) const
{
    if (row < 0 || row >= entries_.size())
        return nullptr;

    return &entries_[row];
}

//--------------------------------------------------------------------------------------------------
int AccessLogListModel::rowOf(qint64 entry_id) const
{
    for (int i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].entry_id() == entry_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int AccessLogListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return static_cast<int>(entries_.size());
}

//--------------------------------------------------------------------------------------------------
int AccessLogListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant AccessLogListModel::data(const QModelIndex& index, int role) const
{
    const proto::router::AccessLogEntry* entry = entryAt(index.row());
    if (!entry || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    switch (role)
    {
        case Qt::DisplayRole:
            return textAt(*entry, column);

        case Qt::DecorationRole:
        {
            if (column != Column::TIME)
                return QVariant();

            const QIcon icon = sessionIcon(entry->session_type());
            if (icon.isNull())
                return QIcon(":/img/unknown.svg");

            return icon;
        }

        case Qt::ToolTipRole:
        {
            // The cell shows the short form of the time, which leaves out the seconds.
            if (column != Column::TIME)
                return QVariant();

            return QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(entry->time()), QLocale::LongFormat);
        }

        default:
            return QVariant();
    }
}

//--------------------------------------------------------------------------------------------------
QVariant AccessLogListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::TIME:
            return tr("Time");

        case Column::USER:
            return tr("User");

        case Column::SESSION_TYPE:
            return tr("Session Type");

        case Column::CLIENT_ADDRESS:
            return tr("Client Address");

        case Column::CLIENT_COMPUTER_NAME:
            return tr("Client Computer");

        case Column::CLIENT_VERSION:
            return tr("Client Version");

        case Column::CLIENT_OS:
            return tr("Client OS");

        case Column::HOST_ID:
            return tr("Host ID");

        case Column::HOST_COMPUTER_NAME:
            return tr("Host Computer");

        case Column::HOST_ADDRESS:
            return tr("Host Address");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void AccessLogListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another entry.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const proto::router::AccessLogEntry* entry = entryAt(old_index.row());
        selected_ids.append(entry ? entry->entry_id() : 0);
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
QString AccessLogListModel::textAt(const proto::router::AccessLogEntry& entry, Column column) const
{
    switch (column)
    {
        case Column::TIME:
            return QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(entry.time()), QLocale::ShortFormat);

        case Column::USER:
            return QString::fromStdString(entry.user_name());

        case Column::SESSION_TYPE:
        {
            // An old client does not say what session it asks the offer for.
            const QString name = sessionName(entry.session_type());
            return name.isEmpty() ? tr("Unknown") : name;
        }

        case Column::CLIENT_ADDRESS:
            return QString::fromStdString(entry.client_address());

        case Column::CLIENT_COMPUTER_NAME:
            return QString::fromStdString(entry.client_computer_name());

        case Column::CLIENT_VERSION:
            return QString::fromStdString(entry.client_version());

        case Column::CLIENT_OS:
            return QString::fromStdString(entry.client_os_name());

        case Column::HOST_ID:
            return hostIdToString(entry.host_id());

        case Column::HOST_COMPUTER_NAME:
            return QString::fromStdString(entry.host_computer_name());

        case Column::HOST_ADDRESS:
            return QString::fromStdString(entry.host_address());
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void AccessLogListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);

    // The time and the host id are compared as the values they are and not as the text they are
    // drawn as. Everything else is compared the way the user reads it, so "host2" comes before
    // "host10".
    auto less = [&](const proto::router::AccessLogEntry& first,
                    const proto::router::AccessLogEntry& second)
    {
        if (column == Column::TIME)
            return first.time() < second.time();

        if (column == Column::HOST_ID)
            return first.host_id() < second.host_id();

        return collator_.compare(textAt(first, column), textAt(second, column)) < 0;
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(entries_.begin(), entries_.end(),
                     [&](const proto::router::AccessLogEntry& first,
                         const proto::router::AccessLogEntry& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
