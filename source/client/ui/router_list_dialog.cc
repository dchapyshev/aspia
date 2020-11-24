//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/router_list_dialog.h"

#include "client/router_config_storage.h"
#include "client/ui/router_dialog.h"
#include "client/ui/router_tree_item.h"

#include <QMessageBox>

namespace client {

RouterListDialog::RouterListDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    reloadRouters();

    connect(ui.tree_routers, &QTreeWidget::currentItemChanged,
            this, &RouterListDialog::currentRouterChanged);

    connect(ui.button_add_router, &QPushButton::released,
            this, &RouterListDialog::addRouter);

    connect(ui.button_modify_router, &QPushButton::released,
            this, &RouterListDialog::modifyRouter);

    connect(ui.button_delete_router, &QPushButton::released,
            this, &RouterListDialog::deleteRouter);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &RouterListDialog::buttonBoxClicked);
}

RouterListDialog::~RouterListDialog() = default;

void RouterListDialog::reloadRouters()
{
    ui.tree_routers->clear();

    std::unique_ptr<RouterConfigStorage> storage = RouterConfigStorage::open();
    if (storage)
    {
        for (const auto& router : storage->routerConfigList())
            ui.tree_routers->addTopLevelItem(new RouterTreeItem(router));
    }
}

void RouterListDialog::currentRouterChanged()
{
    RouterTreeItem* router_item = static_cast<RouterTreeItem*>(ui.tree_routers->currentItem());
    if (!router_item)
    {
        ui.button_modify_router->setEnabled(false);
        ui.button_delete_router->setEnabled(false);
    }
    else
    {
        ui.button_modify_router->setEnabled(true);
        ui.button_delete_router->setEnabled(true);
    }
}

void RouterListDialog::addRouter()
{
    RouterDialog dialog(std::nullopt, this);
    if (dialog.exec() == QDialog::Accepted)
        ui.tree_routers->addTopLevelItem(new RouterTreeItem(dialog.router().value()));
}

void RouterListDialog::modifyRouter()
{
    RouterTreeItem* router_item = static_cast<RouterTreeItem*>(ui.tree_routers->currentItem());
    if (!router_item)
        return;

    RouterDialog dialog(router_item->router(), this);
    if (dialog.exec() == QDialog::Accepted)
        router_item->setRouter(dialog.router().value());
}

void RouterListDialog::deleteRouter()
{
    RouterTreeItem* router_item = static_cast<RouterTreeItem*>(ui.tree_routers->currentItem());
    if (!router_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to remove router \"%1\"?")
                              .arg(router_item->text(0)),
                              QMessageBox::Yes,
                              QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    delete router_item;
}

void RouterListDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        close();
        return;
    }

    RouterConfigList routers;

    for (int i = 0; i < ui.tree_routers->topLevelItemCount(); ++i)
    {
        RouterTreeItem* item = static_cast<RouterTreeItem*>(ui.tree_routers->topLevelItem(i));
        if (item)
            routers.emplace_back(item->router());
    }

    std::unique_ptr<RouterConfigStorage> storage = RouterConfigStorage::open();
    if (storage)
        storage->setRouterConfigList(routers);

    accept();
    close();
}

} // namespace client
