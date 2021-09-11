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

#include "host/ui/config_dialog.h"

#include "base/logging.h"
#include "base/desktop/screen_capturer.h"
#include "base/files/base_paths.h"
#include "base/net/address.h"
#include "base/win/service_controller.h"
#include "build/build_config.h"
#include "common/ui/about_dialog.h"
#include "host/ui/change_password_dialog.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/ui/user_settings.h"
#include "host/ui/settings_util.h"
#include "host/win/service_constants.h"
#include "host/system_settings.h"
#include "common/ui/update_dialog.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>

namespace host {

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(LS_INFO) << "ConfigDialog Ctor";
    ui.setupUi(this);

    //---------------------------------------------------------------------------------------------
    // General Tab
    //---------------------------------------------------------------------------------------------

    connect(ui.spinbox_port, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigDialog::onConfigChanged);

    connect(ui.button_service_install_remove, &QPushButton::clicked,
            this, &ConfigDialog::onServiceInstallRemove);
    connect(ui.button_service_start_stop, &QPushButton::clicked,
            this, &ConfigDialog::onServiceStartStop);

    connect(ui.button_change_password, &QPushButton::clicked,
            this, &ConfigDialog::onChangePassClicked);
    connect(ui.button_pass_protection, &QPushButton::clicked,
            this, &ConfigDialog::onPassProtectClicked);

    connect(ui.button_import, &QPushButton::clicked, this, &ConfigDialog::onImport);
    connect(ui.button_export, &QPushButton::clicked, this, &ConfigDialog::onExport);

    //---------------------------------------------------------------------------------------------
    // Router Tab
    //---------------------------------------------------------------------------------------------

    SystemSettings settings;
    bool is_router_enabled = settings.isRouterEnabled();

    ui.checkbox_enable_router->setChecked(is_router_enabled);
    ui.edit_router_address->setText(QString::fromStdU16String(settings.routerAddress()));
    ui.edit_router_public_key->setPlainText(
        QString::fromStdString(base::toHex(settings.routerPublicKey())));

    ui.label_router_address->setEnabled(is_router_enabled);
    ui.edit_router_address->setEnabled(is_router_enabled);
    ui.label_router_public_key->setEnabled(is_router_enabled);
    ui.edit_router_public_key->setEnabled(is_router_enabled);

    connect(ui.checkbox_enable_router, &QCheckBox::toggled, this, [this](bool checked)
    {
        setConfigChanged(true);
        ui.label_router_address->setEnabled(checked);
        ui.edit_router_address->setEnabled(checked);
        ui.label_router_public_key->setEnabled(checked);
        ui.edit_router_public_key->setEnabled(checked);
    });

    connect(ui.edit_router_address, &QLineEdit::textEdited, this, &ConfigDialog::onConfigChanged);
    connect(ui.edit_router_public_key, &QPlainTextEdit::textChanged, this, &ConfigDialog::onConfigChanged);

    //---------------------------------------------------------------------------------------------
    // Users Tab
    //---------------------------------------------------------------------------------------------

    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &ConfigDialog::onUserContextMenu);

    connect(ui.tree_users, &QTreeWidget::currentItemChanged,
            this, &ConfigDialog::onCurrentUserChanged);

    connect(ui.tree_users, &QTreeWidget::itemDoubleClicked,
            this, &ConfigDialog::onModifyUser);

    connect(ui.action_add, &QAction::triggered, this, &ConfigDialog::onAddUser);
    connect(ui.action_modify, &QAction::triggered, this, &ConfigDialog::onModifyUser);
    connect(ui.action_delete, &QAction::triggered, this, &ConfigDialog::onDeleteUser);
    connect(ui.button_add, &QPushButton::clicked, this, &ConfigDialog::onAddUser);
    connect(ui.button_modify, &QPushButton::clicked, this, &ConfigDialog::onModifyUser);
    connect(ui.button_delete, &QPushButton::clicked, this, &ConfigDialog::onDeleteUser);

    //---------------------------------------------------------------------------------------------
    // Update Tab
    //---------------------------------------------------------------------------------------------

    connect(ui.checkbox_use_custom_server, &QCheckBox::toggled, this, [this](bool checked)
    {
        setConfigChanged(true);

        ui.edit_update_server->setEnabled(checked);

        if (!checked)
            ui.edit_update_server->setText(QString::fromStdU16String(DEFAULT_UPDATE_SERVER));
    });

    connect(ui.edit_update_server, &QLineEdit::textEdited,
            this, &ConfigDialog::onConfigChanged);

    connect(ui.button_check_updates, &QPushButton::clicked, this, [this]()
    {
        common::UpdateDialog(
            QString::fromStdU16String(SystemSettings().updateServer()), "host", this).exec();
    });

    //---------------------------------------------------------------------------------------------
    // Advanced Tab
    //---------------------------------------------------------------------------------------------

    ui.combo_video_capturer->addItem(
        tr("Default"), static_cast<uint32_t>(base::ScreenCapturer::Type::DEFAULT));

#if defined(OS_WIN)
    ui.combo_video_capturer->addItem(
        QStringLiteral("DXGI"), static_cast<uint32_t>(base::ScreenCapturer::Type::WIN_DXGI));

    ui.combo_video_capturer->addItem(
        QStringLiteral("GDI"), static_cast<uint32_t>(base::ScreenCapturer::Type::WIN_GDI));
#elif defined(OS_LINUX)
    ui.combo_video_capturer->addItem(
        QStringLiteral("X11"), static_cast<uint32_t>(base::ScreenCapturer::Type::LINUX_X11));
#elif defined(OS_MAC)
    ui.combo_video_capturer->addItem(
        QStringLiteral("MACOSX"), static_cast<uint32_t>(base::ScreenCapturer::Type::MACOSX));
#else
#error Platform support not implemented
#endif

    int current_video_capturer =
        ui.combo_video_capturer->findData(settings.preferredVideoCapturer());
    if (current_video_capturer != -1)
        ui.combo_video_capturer->setCurrentIndex(current_video_capturer);

    connect(ui.combo_video_capturer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]()
    {
        setConfigChanged(true);
    });

    ui.tab_bar->setTabVisible(4, QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));

    //---------------------------------------------------------------------------------------------
    // Other
    //---------------------------------------------------------------------------------------------

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &ConfigDialog::onButtonBoxClicked);
    reloadAll();
}

ConfigDialog::~ConfigDialog()
{
    LOG(LS_INFO) << "ConfigDialog Dtor";
}

void ConfigDialog::onUserContextMenu(const QPoint& point)
{
    QMenu menu;

    QTreeWidgetItem* current_item = ui.tree_users->itemAt(point);
    if (current_item)
    {
        ui.tree_users->setCurrentItem(current_item);

        menu.addAction(ui.action_modify);
        menu.addAction(ui.action_delete);
        menu.addSeparator();
    }

    menu.addAction(ui.action_add);
    menu.exec(ui.tree_users->viewport()->mapToGlobal(point));
}

void ConfigDialog::onCurrentUserChanged(
    QTreeWidgetItem* /* current */, QTreeWidgetItem* /* previous */)
{
    ui.button_modify->setEnabled(true);
    ui.button_delete->setEnabled(true);

    ui.action_modify->setEnabled(true);
    ui.action_delete->setEnabled(true);
}

void ConfigDialog::onAddUser()
{
    QStringList exist_names;

    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
        exist_names.append(ui.tree_users->topLevelItem(i)->text(0));

    UserDialog dialog(base::User(), exist_names, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        ui.tree_users->addTopLevelItem(new UserTreeItem(dialog.user()));
        setConfigChanged(true);
    }
}

void ConfigDialog::onModifyUser()
{
    UserTreeItem* current_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!current_item)
        return;

    QString current_name = current_item->text(0);
    QStringList exist_names;

    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
    {
        QString name = ui.tree_users->topLevelItem(i)->text(0);
        if (name.compare(current_name, Qt::CaseInsensitive) != 0)
            exist_names.append(name);
    }

    UserDialog dialog(current_item->user(), exist_names, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        current_item->setUser(dialog.user());
        setConfigChanged(true);
    }
}

void ConfigDialog::onDeleteUser()
{
    UserTreeItem* user_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete user \"%1\"?")
                                  .arg(user_item->text(0)),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        delete user_item;
        setConfigChanged(true);
    }
}

void ConfigDialog::onServiceInstallRemove()
{
    switch (service_state_)
    {
        case ServiceState::NOT_INSTALLED:
            installService();
            break;

        case ServiceState::NOT_STARTED:
        case ServiceState::STARTED:
        {
            if (service_state_ == ServiceState::STARTED)
            {
                if (!stopService())
                    break;
            }

            removeService();
        }
        break;

        default:
            break;
    }

    reloadServiceStatus();
}

void ConfigDialog::onServiceStartStop()
{
    switch (service_state_)
    {
        case ServiceState::NOT_STARTED:
            startService();
            break;

        case ServiceState::STARTED:
            stopService();
            break;

        default:
            break;
    }

    reloadServiceStatus();
}

void ConfigDialog::onPassProtectClicked()
{
    SystemSettings settings;

    if (!settings.passwordProtection())
    {
        ChangePasswordDialog dialog(ChangePasswordDialog::Mode::CREATE_NEW_PASSWORD, this);
        if (dialog.exec() == ChangePasswordDialog::Accepted)
        {
            base::ByteArray hash;
            base::ByteArray salt;

            if (!SystemSettings::createPasswordHash(dialog.newPassword().toStdString(), &hash, &salt))
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("An error occurred while processing the password."),
                                     QMessageBox::Ok);
                return;
            }

            settings.setPasswordProtection(true);
            settings.setPasswordHash(hash);
            settings.setPasswordHashSalt(salt);
        }
    }
    else
    {
        CheckPasswordDialog dialog(this);
        if (dialog.exec() == CheckPasswordDialog::Accepted)
        {
            settings.setPasswordProtection(false);
            settings.setPasswordHash(base::ByteArray());
            settings.setPasswordHashSalt(base::ByteArray());
        }
    }

    QTimer::singleShot(0, this, &ConfigDialog::reloadAll);
}

void ConfigDialog::onChangePassClicked()
{
    ChangePasswordDialog dialog(ChangePasswordDialog::Mode::CHANGE_PASSWORD, this);
    if (dialog.exec() == ChangePasswordDialog::Accepted)
    {
        base::ByteArray hash;
        base::ByteArray salt;

        if (!SystemSettings::createPasswordHash(dialog.newPassword().toStdString(), &hash, &salt))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("An error occurred while processing the password."),
                                 QMessageBox::Ok);
            return;
        }

        SystemSettings settings;
        settings.setPasswordProtection(true);
        settings.setPasswordHash(hash);
        settings.setPasswordHashSalt(salt);
    }

    QTimer::singleShot(0, this, &ConfigDialog::reloadAll);
}

void ConfigDialog::onImport()
{
    QString file_path =
        QFileDialog::getOpenFileName(this, tr("Import"), QString(), tr("JSON-files (*.json)"));
    if (file_path.isEmpty())
        return;

    if (SettingsUtil::importFromFile(file_path.toStdU16String(), false, this))
    {
        if (isServiceStarted())
        {
            QString message =
                tr("Service configuration changed. "
                   "For the changes to take effect, you must restart the service. "
                   "Restart the service now?");

            if (QMessageBox::question(this,
                                      tr("Confirmation"),
                                      message,
                                      QMessageBox::Yes,
                                      QMessageBox::No) == QMessageBox::Yes)
            {
                restartService();
            }
        }

        reloadAll();
    }
}

void ConfigDialog::onExport()
{
    QString file_path =
        QFileDialog::getSaveFileName(this, tr("Export"), QString(), tr("JSON-files (*.json)"));
    if (file_path.isEmpty())
        return;

    SettingsUtil::exportToFile(file_path.toStdU16String(), false, this);
}

void ConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);

    if (isConfigChanged() && (standard_button == QDialogButtonBox::Ok ||
                              standard_button == QDialogButtonBox::Apply))
    {
        SystemSettings settings;

        if (!settings.isWritable())
        {
            QString message =
                tr("The configuration can not be written. "
                   "Make sure that you have sufficient rights to write.");

            QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
            return;
        }

        bool service_restart_required = false;

        // Now we have only one parameter when changing which requires restarting the service.
        if (isServiceStarted() && settings.tcpPort() != ui.spinbox_port->value())
        {
            QString message =
                tr("Service configuration changed. "
                   "For the changes to take effect, you must restart the service. "
                   "Restart the service now?");

            if (QMessageBox::question(this,
                                      tr("Confirmation"),
                                      message,
                                      QMessageBox::Yes,
                                      QMessageBox::No) == QMessageBox::Yes)
            {
                service_restart_required = true;
            }
        }

        settings.setRouterEnabled(ui.checkbox_enable_router->isChecked());
        if (ui.checkbox_enable_router->isChecked())
        {
            base::Address router_address = base::Address::fromString(
                ui.edit_router_address->text().toStdU16String(), DEFAULT_ROUTER_TCP_PORT);
            if (!router_address.isValid())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Incorrect router address entered."),
                                     QMessageBox::Ok);
                ui.edit_router_address->setFocus();
                ui.edit_router_address->selectAll();
                return;
            }

            base::ByteArray router_public_key =
                base::fromHex(ui.edit_router_public_key->toPlainText().toStdString());
            if (router_public_key.size() != 32)
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Incorrect router public key entered."),
                                     QMessageBox::Ok);
                ui.edit_router_public_key->setFocus();
                ui.edit_router_public_key->selectAll();
                return;
            }

            settings.setRouterAddress(router_address.host());
            settings.setRouterPort(router_address.port());
            settings.setRouterPublicKey(router_public_key);
        }

        std::unique_ptr<base::UserList> user_list = base::UserList::createEmpty();

        for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
        {
            UserTreeItem* user_tree_item =
                static_cast<UserTreeItem*>(ui.tree_users->topLevelItem(i));

            user_list->add(user_tree_item->user());
        }

        // Update the parameters.
        settings.setTcpPort(static_cast<uint16_t>(ui.spinbox_port->value()));
        settings.setUserList(*user_list);
        settings.setUpdateServer(ui.edit_update_server->text().toStdU16String());
        settings.setPreferredVideoCapturer(ui.combo_video_capturer->currentData().toUInt());
        settings.flush();

        setConfigChanged(false);

        if (service_restart_required)
            restartService();
    }

    if (standard_button == QDialogButtonBox::Apply)
    {
        return;
    }
    else if (standard_button == QDialogButtonBox::Ok)
    {
        accept();
    }
    else if (standard_button == QDialogButtonBox::Cancel)
    {
        reject();
    }

    close();
}

void ConfigDialog::setConfigChanged(bool changed)
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(LS_FATAL) << "Button not found";
        return;
    }

    apply_button->setEnabled(changed);
}

bool ConfigDialog::isConfigChanged() const
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(LS_FATAL) << "Button not found";
        return false;
    }

    return apply_button->isEnabled();
}

void ConfigDialog::reloadAll()
{
    SystemSettings settings;

    reloadServiceStatus();
    reloadUserList(*settings.userList());

    ui.spinbox_port->setValue(settings.tcpPort());
    ui.checkbox_use_custom_server->setChecked(settings.updateServer() != DEFAULT_UPDATE_SERVER);
    ui.edit_update_server->setText(QString::fromStdU16String(settings.updateServer()));

    ui.edit_update_server->setEnabled(ui.checkbox_use_custom_server->isChecked());

    if (!settings.passwordProtection())
    {
        ui.label_pass_protection->setText(tr("Current state: Not installed"));
        ui.button_pass_protection->setText(tr("Install"));
        ui.button_change_password->setVisible(false);
    }
    else
    {
        ui.label_pass_protection->setText(tr("Current state: Installed"));
        ui.button_pass_protection->setText(tr("Remove"));
        ui.button_change_password->setVisible(true);
    }

    setConfigChanged(false);
}

void ConfigDialog::reloadUserList(const base::UserList& user_list)
{
    ui.tree_users->clear();

    for (const auto& user : user_list.list())
        ui.tree_users->addTopLevelItem(new UserTreeItem(user));

    ui.button_modify->setEnabled(false);
    ui.button_delete->setEnabled(false);

    ui.action_modify->setEnabled(false);
    ui.action_delete->setEnabled(false);
}

void ConfigDialog::reloadServiceStatus()
{
    ui.button_service_install_remove->setEnabled(true);
    ui.button_service_start_stop->setEnabled(true);

    QString state;

    if (base::win::ServiceController::isInstalled(kHostServiceName))
    {
        ui.button_service_install_remove->setText(tr("Remove"));

        base::win::ServiceController controller =
            base::win::ServiceController::open(kHostServiceName);
        if (controller.isValid())
        {
            if (controller.isRunning())
            {
                service_state_ = ServiceState::STARTED;
                state = tr("Started");
                ui.button_service_start_stop->setText(tr("Stop"));
            }
            else
            {
                service_state_ = ServiceState::NOT_STARTED;
                state = tr("Not started");
                ui.button_service_start_stop->setText(tr("Start"));
            }
        }
        else
        {
            service_state_ = ServiceState::ACCESS_DENIED;
            state = tr("Installed");
            ui.button_service_start_stop->setText(tr("Start"));

            // The service is installed, but there is no access to management.
            ui.button_service_install_remove->setEnabled(false);
            ui.button_service_start_stop->setEnabled(false);
        }
    }
    else
    {
        service_state_ = ServiceState::NOT_INSTALLED;
        state = tr("Not installed");

        ui.button_service_install_remove->setText(tr("Install"));
        ui.button_service_start_stop->setText(tr("Start"));
        ui.button_service_start_stop->setEnabled(false);
    }

    ui.label_service_status->setText(tr("Current state: %1").arg(state));
}

bool ConfigDialog::isServiceStarted()
{
    base::win::ServiceController controller = base::win::ServiceController::open(kHostServiceName);
    if (controller.isValid())
        return controller.isRunning();

    return false;
}

bool ConfigDialog::installService()
{
    std::filesystem::path service_file_path;

    if (!base::BasePaths::currentExecDir(&service_file_path))
        return false;

    service_file_path.append(kHostServiceFileName);

    base::win::ServiceController controller = base::win::ServiceController::install(
        kHostServiceName, kHostServiceDisplayName, service_file_path);
    if (!controller.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The service could not be installed."),
                             QMessageBox::Ok);
        return false;
    }
    else
    {
        controller.setDependencies({ u"RpcSs", u"Tcpip", u"NDIS", u"AFD" });
        controller.setDescription(kHostServiceDescription);
    }

    return true;
}

bool ConfigDialog::removeService()
{
    if (!base::win::ServiceController::remove(kHostServiceName))
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The service could not be removed."),
                             QMessageBox::Ok);
        return false;
    }

    return true;
}

bool ConfigDialog::startService()
{
    base::win::ServiceController controller = base::win::ServiceController::open(kHostServiceName);
    if (!controller.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not access the service."),
                             QMessageBox::Ok);
        return false;
    }
    else
    {
        if (!controller.start())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be started."),
                                 QMessageBox::Ok);
            return false;
        }
    }

    return true;
}

bool ConfigDialog::stopService()
{
    base::win::ServiceController controller = base::win::ServiceController::open(kHostServiceName);
    if (!controller.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not access the service."),
                             QMessageBox::Ok);
        return false;
    }
    else
    {
        if (!controller.stop())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be stopped."),
                                 QMessageBox::Ok);
            return false;
        }
    }

    return true;
}

bool ConfigDialog::restartService()
{
    if (!stopService())
        return false;

    return startService();
}

} // namespace host
