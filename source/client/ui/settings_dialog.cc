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

#include "client/ui/settings_dialog.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/ui/router_dialog.h"
#include "client/ui/settings.h"
#include "common/ui/update_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    Settings settings;

    // Router tab.
    RouterConfigList routers = settings.routerConfigs();
    for (const auto& config : std::as_const(routers))
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(config.address);
        address.setPort(config.port);

        QTreeWidgetItem* item = new QTreeWidgetItem(ui.tree_routers);
        item->setText(0, address.toString());
        item->setText(1, config.username);
        item->setData(0, Qt::UserRole, config.password);
    }

    ui.tree_routers->header()->setStretchLastSection(true);
    ui.tree_routers->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(ui.button_add_router, &QPushButton::clicked, this, &SettingsDialog::onAddRouter);
    connect(ui.button_edit_router, &QPushButton::clicked, this, &SettingsDialog::onEditRouter);
    connect(ui.button_remove_router, &QPushButton::clicked, this, &SettingsDialog::onRemoveRouter);
    connect(ui.tree_routers, &QTreeWidget::doubleClicked, this, &SettingsDialog::onEditRouter);
    connect(ui.tree_routers, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::updateRouterButtons);

    updateRouterButtons();

    // Other tab.
    ui.edit_display_name->setText(settings.displayName());

    // Desktop tab.
    proto::control::Config desktop_config = settings.desktopConfig();
    ui.checkbox_audio->setChecked(desktop_config.audio());
    ui.checkbox_clipboard->setChecked(desktop_config.clipboard());
    ui.checkbox_cursor_shape->setChecked(desktop_config.cursor_shape());
    ui.checkbox_enable_cursor_pos->setChecked(desktop_config.cursor_position());
    ui.checkbox_desktop_effects->setChecked(!desktop_config.effects());
    ui.checkbox_desktop_wallpaper->setChecked(!desktop_config.wallpaper());
    ui.checkbox_lock_at_disconnect->setChecked(desktop_config.lock_at_disconnect());
    ui.checkbox_block_remote_input->setChecked(desktop_config.block_input());

    // Update tab.
#if defined(Q_OS_WINDOWS)
    ui.checkbox_check_updates->setChecked(settings.checkUpdates());
    ui.edit_update_server->setText(settings.updateServer());

    if (settings.updateServer() == DEFAULT_UPDATE_SERVER)
    {
        ui.checkbox_custom_server->setChecked(false);
        ui.edit_update_server->setEnabled(false);
    }
    else
    {
        ui.checkbox_custom_server->setChecked(true);
        ui.edit_update_server->setEnabled(true);
    }

    connect(ui.checkbox_custom_server, &QCheckBox::toggled, this, [this](bool checked)
    {
        LOG(INFO) << "[ACTION] Custom server checkbox:" << checked;
        ui.edit_update_server->setEnabled(checked);

        if (!checked)
            ui.edit_update_server->setText(DEFAULT_UPDATE_SERVER);
    });

    connect(ui.button_check_for_updates, &QPushButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] Check for updates";
        common::UpdateDialog(ui.edit_update_server->text(), "client", this).exec();
    });
#else
    ui.tabbar->setTabVisible(ui.tabbar->indexOf(ui.tab_update), false);
#endif

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &SettingsDialog::onButtonBoxClicked);
}

//--------------------------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";
    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }
    else
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        // Save router configs.
        RouterConfigList routers;
        for (int i = 0; i < ui.tree_routers->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = ui.tree_routers->topLevelItem(i);

            base::Address address =
                base::Address::fromString(item->text(0), DEFAULT_ROUTER_TCP_PORT);

            RouterConfig config;
            config.address = address.host();
            config.port = address.port();
            config.username = item->text(1);
            config.password = item->data(0, Qt::UserRole).toString();
            routers.append(config);
        }

        Settings settings;
        settings.setRouterConfigs(routers);
        settings.setDisplayName(ui.edit_display_name->text());

        proto::control::Config desktop_config;
        desktop_config.set_audio(ui.checkbox_audio->isChecked());
        desktop_config.set_clipboard(ui.checkbox_clipboard->isChecked());
        desktop_config.set_cursor_shape(ui.checkbox_cursor_shape->isChecked());
        desktop_config.set_cursor_position(ui.checkbox_enable_cursor_pos->isChecked());
        desktop_config.set_effects(!ui.checkbox_desktop_effects->isChecked());
        desktop_config.set_wallpaper(!ui.checkbox_desktop_wallpaper->isChecked());
        desktop_config.set_lock_at_disconnect(ui.checkbox_lock_at_disconnect->isChecked());
        desktop_config.set_block_input(ui.checkbox_block_remote_input->isChecked());
        settings.setDesktopConfig(desktop_config);

#if defined(Q_OS_WINDOWS)
        settings.setCheckUpdates(ui.checkbox_check_updates->isChecked());
        settings.setUpdateServer(ui.edit_update_server->text());
#endif

        accept();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onAddRouter()
{
    LOG(INFO) << "[ACTION] Add router";

    RouterDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        RouterConfig config = dialog.routerConfig();

        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(config.address);
        address.setPort(config.port);

        QTreeWidgetItem* item = new QTreeWidgetItem(ui.tree_routers);
        item->setText(0, address.toString());
        item->setText(1, config.username);
        item->setData(0, Qt::UserRole, config.password);

        ui.tree_routers->setCurrentItem(item);
        updateRouterButtons();
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onEditRouter()
{
    LOG(INFO) << "[ACTION] Edit router";

    QTreeWidgetItem* item = ui.tree_routers->currentItem();
    if (!item)
        return;

    base::Address addr = base::Address::fromString(item->text(0), DEFAULT_ROUTER_TCP_PORT);

    RouterConfig config;
    config.address = addr.host();
    config.port = addr.port();
    config.username = item->text(1);
    config.password = item->data(0, Qt::UserRole).toString();

    RouterDialog dialog(config, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        config = dialog.routerConfig();

        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(config.address);
        address.setPort(config.port);

        item->setText(0, address.toString());
        item->setText(1, config.username);
        item->setData(0, Qt::UserRole, config.password);
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onRemoveRouter()
{
    LOG(INFO) << "[ACTION] Remove router";

    QTreeWidgetItem* item = ui.tree_routers->currentItem();
    if (!item)
        return;

    delete item;
    updateRouterButtons();
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::updateRouterButtons()
{
    bool has_selection = ui.tree_routers->currentItem() != nullptr;
    ui.button_edit_router->setEnabled(has_selection);
    ui.button_remove_router->setEnabled(has_selection);
}

} // namespace client
