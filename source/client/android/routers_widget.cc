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

#include "client/android/routers_widget.h"

#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

//--------------------------------------------------------------------------------------------------
RoutersWidget::RoutersWidget(QWidget* parent)
    : QWidget(parent),
      list_(new TreeWidget(this)),
      add_button_(new IconButton(":/img/add.svg", this)),
      edit_button_(new IconButton(":/img/pencil-drawing.svg", this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(list_, 1);

    reload();
}

//--------------------------------------------------------------------------------------------------
RoutersWidget::~RoutersWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RoutersWidget::appBarActions() const
{
    return { add_button_, edit_button_ };
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::reload()
{
    list_->clear();

    const QIcon icon = GuiApplication::svgIcon(":/img/stack.svg");

    for (const RouterConfig& router : Database::instance().routerList())
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(list_, {router.displayLabel()});
        item->setIcon(0, icon);
        item->setData(0, Qt::UserRole, router.routerId());
    }
}
