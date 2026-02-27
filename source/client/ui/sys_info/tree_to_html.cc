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

#include "client/ui/sys_info/tree_to_html.h"

#include <QFile>
#include <QTreeWidget>
#include <QXmlStreamWriter>

namespace client {

namespace {

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

//--------------------------------------------------------------------------------------------------
void tableHeader(QXmlStreamWriter* writer, const QTreeWidget* tree)
{
    writer->writeStartElement("tr");

    QTreeWidgetItem* header = tree->headerItem();
    for (int i = 0; i < tree->columnCount(); ++i)
    {
        writer->writeStartElement("th");
        writer->writeCharacters(header->text(i));
        writer->writeEndElement(); // th
    }

    writer->writeEndElement(); // tr
}

//--------------------------------------------------------------------------------------------------
void tableRow(QXmlStreamWriter* writer,
              const QTreeWidget* tree,
              const QTreeWidgetItem* item,
              int padding)
{
    writer->writeStartElement("tr");

    for (int i = 0; i < tree->columnCount(); ++i)
    {
        writer->writeStartElement("td");

        if (i == 0)
        {
            QString style;

            if (padding)
                style = "padding-left: " + QString::number(padding) + "px;";
            else
                style = "font-weight: bold;";

            writer->writeAttribute("style", style);
        }

        writer->writeCharacters(item->text(i));
        writer->writeEndElement(); // td
    }

    writer->writeEndElement(); // tr

    for (int i = 0; i < item->childCount(); ++i)
        tableRow(writer, tree, item->child(i), padding + 15);
}

//--------------------------------------------------------------------------------------------------
void treeToHtml(const QTreeWidget* tree, QXmlStreamWriter* writer)
{
    beginDocument(writer);

    writer->writeStartElement("table");
    writer->writeAttribute("cellspacing", "0");
    writer->writeAttribute("cellpadding", "0");
    writer->writeAttribute("border", "1");

    tableHeader(writer, tree);

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        tableRow(writer, tree, tree->topLevelItem(i), 0);

    writer->writeEndElement(); // table

    endDocument(writer);
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString treeToHtmlString(const QTreeWidget* tree)
{
    QString string;

    QXmlStreamWriter writer(&string);
    writer.setAutoFormatting(false);

    treeToHtml(tree, &writer);
    return string;
}

//--------------------------------------------------------------------------------------------------
bool treeToHtmlFile(const QTreeWidget* tree, const QString& file_path, QString* error_string)
{
    QFile file(file_path);

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

    treeToHtml(tree, &writer);
    return true;
}

} // namespace client
