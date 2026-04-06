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

#include "client/ui/computers_tab/local_group_widget.h"

#include "base/logging.h"
#include "client/book_database.h"
#include "client/ui/computers_tab/content_tree_item.h"

namespace client {

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::LocalGroupWidget(BookDatabase* database, QWidget* parent)
    : ContentWidget(Type::LOCAL_GROUP, parent),
      database_(database)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    connect(ui.tree_computer, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        ContentTreeItem* computer_item = static_cast<ContentTreeItem*>(item);
        emit sig_computerDoubleClicked(computer_item->computerId());
    });
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::~LocalGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
LocalComputerItem* LocalGroupWidget::currentComputer()
{
    return static_cast<LocalComputerItem*>(ui.tree_computer->currentItem());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    ui.tree_computer->clear();

    QList<ComputerData> computers = database_->computerList(group_id);

    for (const ComputerData& computer : std::as_const(computers))
        new LocalComputerItem(computer, ui.tree_computer);
}

//--------------------------------------------------------------------------------------------------
int LocalGroupWidget::itemCount() const
{
    return ui.tree_computer->topLevelItemCount();
}

} // namespace client
