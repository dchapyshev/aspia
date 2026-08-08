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

#include "client/desktop/management/host_list_model.h"

#include <QDateTime>
#include <QIcon>
#include <QLocale>

#include <algorithm>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString timestampToString(qint64 timestamp)
{
    if (timestamp <= 0)
        return QString();

    return QLocale::system().toString(
        QDateTime::fromSecsSinceEpoch(timestamp), QLocale::ShortFormat);
}

//--------------------------------------------------------------------------------------------------
// An unnamed host is shown by the name of the computer it runs on.
QString hostName(const RouterHost& host)
{
    return host.display_name.isEmpty() ? host.computer_name : host.display_name;
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostListModel::HostListModel(QList<Column> columns, QObject* parent)
    : QAbstractTableModel(parent),
      columns_(std::move(columns))
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);

    // Whether the host is connected is drawn once, on the leftmost of the two columns that name
    // the host.
    const int host_id_section = static_cast<int>(columns_.indexOf(Column::HOST_ID));
    const int name_section = static_cast<int>(columns_.indexOf(Column::DISPLAY_NAME));

    if (host_id_section < 0)
        icon_section_ = name_section;
    else if (name_section < 0)
        icon_section_ = host_id_section;
    else
        icon_section_ = qMin(host_id_section, name_section);
}

//--------------------------------------------------------------------------------------------------
HostListModel::~HostListModel() = default;

//--------------------------------------------------------------------------------------------------
void HostListModel::setHosts(const QList<RouterHost>& hosts)
{
    beginResetModel();
    hosts_ = hosts;
    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
void HostListModel::setWorkspaceNames(const QHash<qint64, QString>& names)
{
    workspace_names_ = names;

    const int workspace_section = static_cast<int>(columns_.indexOf(Column::WORKSPACE));
    const int last_row = rowCount() - 1;
    if (workspace_section < 0 || last_row < 0)
        return;

    emit dataChanged(index(0, workspace_section), index(last_row, workspace_section),
                     { Qt::DisplayRole });
}

//--------------------------------------------------------------------------------------------------
const RouterHost* HostListModel::hostAt(int row) const
{
    if (row < 0 || row >= hosts_.size())
        return nullptr;

    return &hosts_[row];
}

//--------------------------------------------------------------------------------------------------
int HostListModel::rowOf(HostId host_id) const
{
    for (int i = 0; i < hosts_.size(); ++i)
    {
        if (hosts_[i].host_id == host_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
HostListModel::Column HostListModel::columnAt(int section) const
{
    CHECK(section >= 0 && section < columns_.size());
    return columns_[section];
}

//--------------------------------------------------------------------------------------------------
int HostListModel::sectionOf(Column column) const
{
    return static_cast<int>(columns_.indexOf(column));
}

//--------------------------------------------------------------------------------------------------
int HostListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(hosts_.size());
}

//--------------------------------------------------------------------------------------------------
int HostListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return static_cast<int>(columns_.size());
}

//--------------------------------------------------------------------------------------------------
QVariant HostListModel::data(const QModelIndex& index, int role) const
{
    const RouterHost* host = hostAt(index.row());
    if (!host || index.column() < 0 || index.column() >= columns_.size())
        return QVariant();

    if (role == Qt::DecorationRole)
    {
        if (index.column() != icon_section_)
            return QVariant();

        return QIcon(host->online ? ":/img/computer-online.svg" : ":/img/computer-offline.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*host, columns_[index.column()]);
}

//--------------------------------------------------------------------------------------------------
QVariant HostListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    if (section < 0 || section >= columns_.size())
        return QVariant();

    switch (columns_[section])
    {
        case Column::HOST_ID:
            return tr("Host ID");

        case Column::DISPLAY_NAME:
            return tr("Display Name");

        case Column::COMPUTER_NAME:
            return tr("Computer Name");

        case Column::ADDRESS:
            return tr("Address");

        case Column::COMMENT:
            return tr("Comment");

        case Column::WORKSPACE:
            return tr("Workspace");

        case Column::OS:
            return tr("Operating System");

        case Column::VERSION:
            return tr("Version");

        case Column::ARCH:
            return tr("Architecture");

        case Column::LAST_CONNECT:
            return tr("Last Connect");

        case Column::LAST_MODIFY:
            return tr("Last Modify");

        case Column::STATUS:
            return tr("Status");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void HostListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= columns_.size())
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on whatever host sorted into it.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<HostId> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const RouterHost* host = hostAt(old_index.row());
        selected_ids.append(host ? host->host_id : kInvalidHostId);
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
QString HostListModel::textAt(const RouterHost& host, Column column) const
{
    switch (column)
    {
        case Column::HOST_ID:
            return QString::number(host.host_id);

        case Column::DISPLAY_NAME:
            return hostName(host);

        case Column::COMPUTER_NAME:
            return host.computer_name;

        case Column::ADDRESS:
            return host.address;

        case Column::COMMENT:
            return host.comment;

        case Column::WORKSPACE:
            return workspace_names_.value(host.workspace_id);

        case Column::OS:
            return host.os_name;

        case Column::VERSION:
            return host.version;

        case Column::ARCH:
            return host.cpu_arch;

        case Column::LAST_CONNECT:
            return timestampToString(host.last_connect);

        case Column::LAST_MODIFY:
            return timestampToString(host.last_modify);

        case Column::STATUS:
            return host.online ? tr("Online") : tr("Offline");
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void HostListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= columns_.size())
        return;

    const Column column = columns_[sort_column_];

    // An id and a timestamp are compared as the numbers they are and not as the text they are drawn
    // as, or 10 would come before 9 and a date would sort by the order its parts are written in.
    // A name is compared the way the user reads it, so "host2" comes before "host10".
    auto less = [&](const RouterHost& first, const RouterHost& second)
    {
        switch (column)
        {
            case Column::HOST_ID:
                return first.host_id < second.host_id;

            case Column::LAST_CONNECT:
                return first.last_connect < second.last_connect;

            case Column::LAST_MODIFY:
                return first.last_modify < second.last_modify;

            case Column::DISPLAY_NAME:
                return collator_.compare(hostName(first), hostName(second)) < 0;

            default:
                return textAt(first, column) < textAt(second, column);
        }
    };

    const bool ascending = sort_order_ == Qt::AscendingOrder;

    std::stable_sort(hosts_.begin(), hosts_.end(),
                     [&](const RouterHost& first, const RouterHost& second)
    {
        return ascending ? less(first, second) : less(second, first);
    });
}
