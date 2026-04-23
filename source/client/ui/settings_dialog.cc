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
#include <QCoreApplication>
#include <QPushButton>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "common/ui/msg_box.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/settings.h"
#include "client/ui/application.h"
#include "client/ui/router_dialog.h"
#include "common/ui/update_dialog.h"

namespace client {

namespace {

const int kColumnAddress     = 0;
const int kColumnName        = 1;
const int kColumnSessionType = 2;
const int kColumnUserName    = 3;

//--------------------------------------------------------------------------------------------------
QString sessionTypeToString(proto::router::SessionType session_type)
{
    switch (session_type)
    {
        case proto::router::SESSION_TYPE_ADMIN:
            return QCoreApplication::translate("SettingsDialog", "Administrator");
        case proto::router::SESSION_TYPE_MANAGER:
            return QCoreApplication::translate("SettingsDialog", "Manager");
        case proto::router::SESSION_TYPE_CLIENT:
            return QCoreApplication::translate("SettingsDialog", "Client");
        default:
            return QCoreApplication::translate("SettingsDialog", "Unknown");
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    Settings settings;

    // Router tab.
    RouterConfigList routers = settings.routerConfigs();
    for (const auto& config : std::as_const(routers))
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(config.address);
        address.setPort(config.port);

        QTreeWidgetItem* item = new QTreeWidgetItem(ui.tree_routers);
        item->setText(kColumnAddress, address.toString());
        item->setIcon(kColumnAddress, QIcon(":/img/stack.svg"));
        item->setText(kColumnName, config.name);
        item->setText(kColumnSessionType, sessionTypeToString(config.session_type));
        item->setText(kColumnUserName, config.username);
        item->setData(kColumnAddress, Qt::UserRole, config.password);
        item->setData(kColumnAddress, Qt::UserRole + 1, config.session_type);
    }

    ui.tree_routers->header()->setStretchLastSection(true);
    ui.tree_routers->header()->setSectionResizeMode(kColumnAddress, QHeaderView::Stretch);
    ui.tree_routers->header()->setSectionResizeMode(kColumnName, QHeaderView::Stretch);

    connect(ui.button_add_router, &QPushButton::clicked, this, &SettingsDialog::onAddRouter);
    connect(ui.button_edit_router, &QPushButton::clicked, this, &SettingsDialog::onEditRouter);
    connect(ui.button_remove_router, &QPushButton::clicked, this, &SettingsDialog::onRemoveRouter);
    connect(ui.tree_routers, &QTreeWidget::doubleClicked, this, &SettingsDialog::onEditRouter);
    connect(ui.tree_routers, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::updateRouterButtons);

    updateRouterButtons();

    // General tab.
    QString current_locale = settings.locale();
    Application::LocaleList locale_list = base::GuiApplication::instance()->localeList();
    for (const auto& locale : std::as_const(locale_list))
    {
        ui.combo_language->addItem(locale.second, locale.first);
        if (locale.first == current_locale)
            ui.combo_language->setCurrentIndex(ui.combo_language->count() - 1);
    }

    QString current_theme = settings.theme();
    QStringList available_themes = base::GuiApplication::instance()->availableThemes();
    for (const QString& theme_id : std::as_const(available_themes))
    {
        ui.combo_theme->addItem(base::GuiApplication::themeName(theme_id), theme_id);
        if (theme_id == current_theme)
            ui.combo_theme->setCurrentIndex(ui.combo_theme->count() - 1);
    }

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
                base::Address::fromString(item->text(kColumnAddress), DEFAULT_ROUTER_TCP_PORT);

            RouterConfig config;
            config.name = item->text(kColumnName);
            config.address = address.host();
            config.port = address.port();
            config.session_type = static_cast<proto::router::SessionType>(
                item->data(kColumnAddress, Qt::UserRole + 1).toInt());
            config.username = item->text(kColumnUserName);
            config.password = item->data(kColumnAddress, Qt::UserRole).toString();
            routers.append(config);
        }

        Settings settings;
        settings.setRouterConfigs(routers);

        // Save language.
        QString new_locale = ui.combo_language->currentData().toString();
        settings.setLocale(new_locale);
        client::Application::instance()->setLocale(new_locale);

        // Save theme.
        QString new_theme = ui.combo_theme->currentData().toString();
        settings.setTheme(new_theme);
        base::GuiApplication::instance()->setTheme(new_theme);

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
        item->setText(kColumnAddress, address.toString());
        item->setIcon(kColumnAddress, QIcon(":/img/stack.svg"));
        item->setText(kColumnName, config.name);
        item->setText(kColumnSessionType, sessionTypeToString(config.session_type));
        item->setText(kColumnUserName, config.username);
        item->setData(kColumnAddress, Qt::UserRole, config.password);
        item->setData(kColumnAddress, Qt::UserRole + 1, config.session_type);

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

    base::Address addr = base::Address::fromString(item->text(kColumnAddress), DEFAULT_ROUTER_TCP_PORT);

    RouterConfig config;
    config.name = item->text(kColumnName);
    config.address = addr.host();
    config.port = addr.port();
    config.session_type = static_cast<proto::router::SessionType>(
        item->data(kColumnAddress, Qt::UserRole + 1).toInt());
    config.username = item->text(kColumnUserName);
    config.password = item->data(kColumnAddress, Qt::UserRole).toString();

    RouterDialog dialog(config, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        config = dialog.routerConfig();

        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(config.address);
        address.setPort(config.port);

        item->setText(kColumnAddress, address.toString());
        item->setText(kColumnName, config.name);
        item->setText(kColumnSessionType, sessionTypeToString(config.session_type));
        item->setText(kColumnUserName, config.username);
        item->setData(kColumnAddress, Qt::UserRole, config.password);
        item->setData(kColumnAddress, Qt::UserRole + 1, config.session_type);
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::onRemoveRouter()
{
    LOG(INFO) << "[ACTION] Remove router";

    QTreeWidgetItem* item = ui.tree_routers->currentItem();
    if (!item)
        return;

    QString display_name = item->text(kColumnName).isEmpty()
        ? item->text(kColumnAddress) : item->text(kColumnName);
    if (common::MsgBox::question(this, tr("Are you sure you want to delete router \"%1\"?")
            .arg(display_name)) == common::MsgBox::Yes)
    {
        delete item;
        updateRouterButtons();
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::showError(const QString& message)
{
    common::MsgBox::warning(this, message);
}

//--------------------------------------------------------------------------------------------------
void SettingsDialog::updateRouterButtons()
{
    bool has_selection = ui.tree_routers->currentItem() != nullptr;
    ui.button_edit_router->setEnabled(has_selection);
    ui.button_remove_router->setEnabled(has_selection);
}

} // namespace client
