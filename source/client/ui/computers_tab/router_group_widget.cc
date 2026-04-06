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

#include "client/ui/computers_tab/router_group_widget.h"

#include "base/logging.h"
#include "client/ui/computers_tab/content_tree_item.h"

#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace client {

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::RouterGroupWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_GROUP, parent)
{
    LOG(INFO) << "Ctor";

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_computer_ = new QTreeWidget(this);
    tree_computer_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_computer_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_computer_->setIndentation(0);
    tree_computer_->setSortingEnabled(true);
    tree_computer_->setColumnCount(3);

    QStringList headers;
    headers << tr("Name") << tr("Address / ID") << tr("Comment");
    tree_computer_->setHeaderLabels(headers);

    layout->addWidget(tree_computer_);

    connect(tree_computer_, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        ContentTreeItem* computer_item = static_cast<ContentTreeItem*>(item);
        emit sig_computerDoubleClicked(computer_item->computerId());
    });
}

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::~RouterGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::showGroup(qint64 /* group_id */)
{
    // TODO: Load computers from router.
    tree_computer_->clear();
}

//--------------------------------------------------------------------------------------------------
int RouterGroupWidget::itemCount() const
{
    return tree_computer_->topLevelItemCount();
}

} // namespace client
