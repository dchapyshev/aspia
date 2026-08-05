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

#ifndef CLIENT_DESKTOP_MANAGEMENT_SEARCH_RESULT_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_SEARCH_RESULT_MODEL_H

#include <QAbstractTableModel>
#include <QCollator>

#include "client/config.h"

// One page of the matches of a search, over the address book and over every online router at once.
// A row of either kind is shown and connected to the same way; what tells them apart is whether
// there is a record behind the row to edit.
class SearchResultModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        NAME,
        ADDRESS,
        SOURCE,
        COMMENT
    };

    enum class Type
    {
        LOCAL,
        ROUTER
    };

    struct Row
    {
        Type type = Type::LOCAL;

        // A local row carries the record as it is stored, a router row the same fields taken from
        // the reply. The id of a router row stays -1: there is no record behind it.
        HostConfig host;

        // The group path in the address book, or the label of the router that answered.
        QString source;
    };

    explicit SearchResultModel(QObject* parent = nullptr);
    ~SearchResultModel() final;

    // Replaces the shown page. The sort the user picked is applied to it.
    void setRows(const QList<Row>& rows);
    void clear();

    // Null when the row is out of range.
    const Row* rowAt(int row) const;

    // Local rows only: a router host has no record of its own and no entry id.
    int rowOfEntry(qint64 entry_id) const;
    bool updateEntry(const HostConfig& host, const QString& source);
    bool removeEntry(qint64 entry_id);

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) final;

private:
    // A row of a router has no id of its own, so the rows are carried across a sort by a key the
    // model hands out instead.
    struct Entry
    {
        Row row;
        quint64 key = 0;
    };

    QString textAt(const Row& row, Column column) const;
    void applySort();
    int rowOfKey(quint64 key) const;

    QList<Entry> entries_;
    quint64 next_key_ = 1;

    // Kept here rather than built per comparison: it carries an ICU collator.
    QCollator collator_;

    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;

    Q_DISABLE_COPY_MOVE(SearchResultModel)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_SEARCH_RESULT_MODEL_H
