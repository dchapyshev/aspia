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

#ifndef CLIENT_DESKTOP_MANAGEMENT_HOST_LIST_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_HOST_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>
#include <QHash>

#include "client/router_types.h"

// The page of hosts a view is showing. The list itself comes from the router a page at a time, so
// the model holds exactly what was answered and nothing more.
//
// The views differ in which columns they show and in which order, so the layout is given at
// construction instead of being fixed here.
class HostListModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        HOST_ID,
        DISPLAY_NAME,
        COMPUTER_NAME,
        ADDRESS,
        USER_NAME,
        COMMENT,
        WORKSPACE,
        OS,
        VERSION,
        ARCH,
        LAST_CONNECT,
        LAST_MODIFY,
        STATUS
    };

    HostListModel(QList<Column> columns, QObject* parent = nullptr);
    ~HostListModel() final;

    // Replaces the page. The sort the user picked is applied to it.
    void setHosts(const QList<RouterHost>& hosts);

    // Names for the workspace column, by workspace entry_id. Views without that column do not
    // need them.
    void setWorkspaceNames(const QHash<qint64, QString>& names);

    // Null when the row is out of range.
    const RouterHost* hostAt(int row) const;

    // Row of the host with this id, or -1. Used to keep the selection across a refetch, which
    // arrives as a new page and not as an edit of the old one.
    int rowOf(HostId host_id) const;

    Column columnAt(int section) const;

    // Section the column is shown at, or -1 when this view does not show it.
    int sectionOf(Column column) const;

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    QString textAt(const RouterHost& host, Column column) const;
    void applySort();

    const QList<Column> columns_;
    QList<RouterHost> hosts_;
    QHash<qint64, QString> workspace_names_;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    // The section the state of the host is drawn on, or -1 when the view names the host by neither
    // its id nor its name.
    int icon_section_ = -1;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(HostListModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_HOST_LIST_MODEL_H
