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

#include "common/sys_info/sys_info_report.h"

#include <QSaveFile>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QXmlStreamWriter>

namespace {

// Indentation a nested row is shifted by.
constexpr int kNestingIndent = 15;

//--------------------------------------------------------------------------------------------------
void beginDocument(QXmlStreamWriter* writer)
{
    QString style =
        "body { font-family: Tahoma, Arial, Verdana; font-size:12px; }"
        "h1 { font-size:14px;  }"
        "td, th { padding:3px; border:1px solid black; }"
        "th { font-weight:bold; }";

    writer->writeStartElement("html");
    writer->writeStartElement("head");

    writer->writeStartElement("meta");
    writer->writeAttribute("http-equiv", "Content-Type");
    writer->writeAttribute("content", "text/html; charset=utf-8");
    writer->writeEndElement(); // meta

    writer->writeStartElement("style");
    writer->writeAttribute("type", "text/css");
    writer->writeCharacters(style);
    writer->writeEndElement(); // style

    writer->writeStartElement("title");
    writer->writeCharacters("Document");
    writer->writeEndElement(); // title

    writer->writeEndElement(); // head

    writer->writeStartElement("body");
}

//--------------------------------------------------------------------------------------------------
void endDocument(QXmlStreamWriter* writer)
{
    writer->writeEndElement(); // body
    writer->writeEndElement(); // html
}

} // namespace

//--------------------------------------------------------------------------------------------------
void SysInfoReport::addTree(const QString& title, const QTreeWidget* tree)
{
    if (!tree || !tree->topLevelItemCount())
        return;

    const int column_count = tree->columnCount();

    Table table;
    table.title = title;

    QTreeWidgetItem* header = tree->headerItem();
    if (header)
    {
        for (int i = 0; i < column_count; ++i)
            table.header << header->text(i);
    }

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        addRow(&table, tree->topLevelItem(i), column_count, 0);

    tables_ << table;
}

//--------------------------------------------------------------------------------------------------
void SysInfoReport::addItems(const QString& title, const QStringList& header,
                             const QList<QTreeWidgetItem*>& items)
{
    if (items.isEmpty())
        return;

    Table table;
    table.title = title;
    table.header = header;

    for (const QTreeWidgetItem* item : items)
        addRow(&table, item, header.count(), 0);

    tables_ << table;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoReport::toString() const
{
    QString string;

    QXmlStreamWriter writer(&string);
    writer.setAutoFormatting(false);

    write(&writer);
    return string;
}

//--------------------------------------------------------------------------------------------------
bool SysInfoReport::save(const QString& file_path, QString* error_string) const
{
    QSaveFile file(file_path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (error_string)
            *error_string = file.errorString();
        return false;
    }

    QXmlStreamWriter writer;
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(-1);
    writer.setDevice(&file);

    write(&writer);

    if (!file.commit())
    {
        if (error_string)
            *error_string = file.errorString();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void SysInfoReport::addRow(Table* table, const QTreeWidgetItem* item, int column_count, int level)
{
    Row row;
    row.level = level;

    for (int i = 0; i < column_count; ++i)
        row.columns << item->text(i);

    table->rows << row;

    for (int i = 0; i < item->childCount(); ++i)
        addRow(table, item->child(i), column_count, level + 1);
}

//--------------------------------------------------------------------------------------------------
void SysInfoReport::write(QXmlStreamWriter* writer) const
{
    beginDocument(writer);

    bool first = true;

    for (const Table& table : tables_)
    {
        // A heading of its own separates a table from the one before it, a table without a heading
        // needs a break for that.
        if (!first && table.title.isEmpty())
            writer->writeEmptyElement("br");

        first = false;

        if (!table.title.isEmpty())
        {
            writer->writeStartElement("h1");
            writer->writeCharacters(table.title);
            writer->writeEndElement(); // h1
        }

        writer->writeStartElement("table");
        writer->writeAttribute("cellspacing", "0");
        writer->writeAttribute("cellpadding", "0");
        writer->writeAttribute("border", "1");

        if (!table.header.isEmpty())
        {
            writer->writeStartElement("tr");

            for (const QString& column : table.header)
            {
                writer->writeStartElement("th");
                writer->writeCharacters(column);
                writer->writeEndElement(); // th
            }

            writer->writeEndElement(); // tr
        }

        for (const Row& row : table.rows)
        {
            writer->writeStartElement("tr");

            for (int i = 0; i < row.columns.count(); ++i)
            {
                writer->writeStartElement("td");

                // The first column carries the nesting: a row of the top level is a heading for the
                // rows under it.
                if (i == 0)
                {
                    writer->writeAttribute("style", row.level ?
                        "padding-left: " + QString::number(row.level * kNestingIndent) + "px;" :
                        QString("font-weight: bold;"));
                }

                writer->writeCharacters(row.columns[i]);
                writer->writeEndElement(); // td
            }

            writer->writeEndElement(); // tr
        }

        writer->writeEndElement(); // table
    }

    endDocument(writer);
}
