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

#ifndef COMMON_SYS_INFO_SYS_INFO_REPORT_H
#define COMMON_SYS_INFO_SYS_INFO_REPORT_H

#include <QList>
#include <QString>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;
class QXmlStreamWriter;

// Report of a page as a set of tables, each with a heading of its own. The page fills it itself:
// the panes it is browsed with show one thing at a time, and a report is expected to hold all of it.
class SysInfoReport
{
public:
    SysInfoReport() = default;
    ~SysInfoReport() = default;

    // Adds a table with the header and the contents of |tree|.
    void addTree(const QString& title, const QTreeWidget* tree);

    // Adds a table of |items| with |header| as its header row. The children of an item are rows of
    // the same table, indented the way they are nested. The items are only read, they stay with
    // the caller.
    void addItems(const QString& title, const QStringList& header,
                  const QList<QTreeWidgetItem*>& items);

    bool isEmpty() const { return tables_.isEmpty(); }

    // The report as an HTML document.
    QString toString() const;

    // Writes the report to an HTML file.
    bool save(const QString& file_path, QString* error_string = nullptr) const;

private:
    // Row of a table. |level| is the depth the row is nested at.
    struct Row
    {
        QStringList columns;
        int level = 0;
    };

    struct Table
    {
        QString title;
        QStringList header;
        QList<Row> rows;
    };

    // Adds |item| and everything under it to the rows of |table|.
    static void addRow(Table* table, const QTreeWidgetItem* item, int column_count, int level);

    void write(QXmlStreamWriter* writer) const;

    QList<Table> tables_;

    Q_DISABLE_COPY_MOVE(SysInfoReport)
};

#endif // COMMON_SYS_INFO_SYS_INFO_REPORT_H
