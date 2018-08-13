//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_list.h"

#include <QKeyEvent>
#include <QProxyStyle>

#include "client/ui/file_item_delegate.h"

namespace aspia {

namespace {

class TreeViewProxyStyle : public QProxyStyle
{
public:
    explicit TreeViewProxyStyle(QStyle* style)
        : QProxyStyle(style)
    {
        // Nothing
    }

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption* option,
                       QPainter* painter,
                       const QWidget* widget) const override
    {
        if (element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull())
        {
            QStyleOption option_dup(*option);

            if (widget)
            {
                option_dup.rect.setX(0);
                option_dup.rect.setWidth(widget->width() - 3);
            }

            QProxyStyle::drawPrimitive(element, &option_dup, painter, widget);
            return;
        }

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

} // namespace

FileList::FileList(QWidget* parent)
    : QTreeView(parent)
{
    setStyle(new TreeViewProxyStyle(style()));
    setItemDelegate(new FileItemDelegate(this));
}

void FileList::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F2)
    {
        QModelIndexList list = selectionModel()->selectedRows();
        if (list.count() == 1)
        {
            edit(list.constFirst());
            event->ignore();
        }
    }

    QTreeView::keyPressEvent(event);
}

} // namespace aspia
