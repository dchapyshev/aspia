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

#include "client/desktop/sys_info/sys_info_widget.h"

#include "proto/system_info.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QHeaderView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>

namespace {

//--------------------------------------------------------------------------------------------------
void copyTextToClipboard(const QString& text)
{
    if (text.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(text);
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidget::SysInfoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
proto::system_info::SystemInfoRequest SysInfoWidget::request() const
{
    proto::system_info::SystemInfoRequest system_info_request;
    system_info_request.set_category(category());
    return system_info_request;
}

//--------------------------------------------------------------------------------------------------
QByteArray SysInfoWidget::saveState() const
{
    QTreeWidget* tree = const_cast<SysInfoWidget*>(this)->treeWidget();
    if (!tree)
        return QByteArray();

    return tree->header()->saveState();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::restoreState(const QByteArray& state)
{
    if (state.isEmpty())
        return;

    QTreeWidget* tree = treeWidget();
    if (!tree)
        return;

    tree->header()->restoreState(state);
    state_restored_ = true;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyRow(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QString result;

    int column_count = item->columnCount();
    if (column_count > 2)
    {
        for (int i = 0; i < column_count; ++i)
        {
            QString text = item->text(i);

            if (!text.isEmpty())
                result += text + ' ';
        }

        result.chop(1);
    }
    else
    {
        result = item->text(0) + ": " + item->text(1);
    }

    copyTextToClipboard(result);
}

//--------------------------------------------------------------------------------------------------
// static
void SysInfoWidget::searchInGoogle(const QString& request)
{
    QUrl find_url("https://www.google.com/search?q=" +
                  QString::fromLatin1(QUrl::toPercentEncoding(request)));
    QDesktopServices::openUrl(find_url);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyColumn(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    copyTextToClipboard(item->text(column));
}
