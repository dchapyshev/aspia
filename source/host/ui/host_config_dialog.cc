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

#include "host/ui/host_config_dialog.h"
#include "base/logging.h"
#include "base/service_controller.h"
#include "base/xml_settings.h"
#include "build/build_config.h"
#include "common/ui/about_dialog.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/win/host_service_constants.h"
#include "host/host_settings.h"
#include "net/srp_user.h"
#include "updater/update_dialog.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTranslator>

namespace host {

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.spinbox_port, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigDialog::onConfigChanged);

    connect(ui.checkbox_add_firewall_rule, &QCheckBox::toggled,
            this, &ConfigDialog::onConfigChanged);

    connect(ui.checkbox_use_custom_server, &QCheckBox::toggled, [this](bool checked)
    {
        setConfigChanged(true);

        ui.edit_update_server->setEnabled(checked);

        if (!checked)
            ui.edit_update_server->setText(DEFAULT_UPDATE_SERVER);
    });

    connect(ui.edit_update_server, &QLineEdit::textEdited,
            this, &ConfigDialog::onConfigChanged);

    connect(ui.button_check_updates, &QPushButton::released, [this]()
    {
        updater::UpdateDialog(Settings().updateServer(), QLatin1String("host"), this).exec();
    });

    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &ConfigDialog::onUserContextMenu);

    connect(ui.tree_users, &QTreeWidget::currentItemChanged,
            this, &ConfigDialog::onCurrentUserChanged);

    connect(ui.tree_users, &QTreeWidget::itemDoubleClicked,
            this, &ConfigDialog::onModifyUser);

    connect(ui.action_add, &QAction::triggered, this, &ConfigDialog::onAddUser);
    connect(ui.action_modify, &QAction::triggered, this, &ConfigDialog::onModifyUser);
    connect(ui.action_delete, &QAction::triggered, this, &ConfigDialog::onDeleteUser);
    connect(ui.button_add, &QPushButton::released, this, &ConfigDialog::onAddUser);
    connect(ui.button_modify, &QPushButton::released, this, &ConfigDialog::onModifyUser);
    connect(ui.button_delete, &QPushButton::released, this, &ConfigDialog::onDeleteUser);

    connect(ui.button_service_install_remove, &QPushButton::released,
            this, &ConfigDialog::onServiceInstallRemove);
    connect(ui.button_service_start_stop, &QPushButton::released,
            this, &ConfigDialog::onServiceStartStop);

    connect(ui.button_import, &QPushButton::released, this, &ConfigDialog::onImport);
    connect(ui.button_export, &QPushButton::released, this, &ConfigDialog::onExport);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ConfigDialog::onButtonBoxClicked);

    reloadAll();
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
    net::SrpUser user;
    user.flags = net::SrpUser::ENABLED;

    if (UserDialog(users_, &user, this).exec() == QDialog::Accepted)
    {
        users_.list.push_back(std::move(user));
        setConfigChanged(true);
        reloadUserList();
    }
}

void ConfigDialog::onModifyUser()
{
    UserTreeItem* user_item = dynamic_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    net::SrpUser* user = &users_.list[user_item->userIndex()];
    if (UserDialog(users_, user, this).exec() == QDialog::Accepted)
    {
        setConfigChanged(true);
        reloadUserList();
    }
}

void ConfigDialog::onDeleteUser()
{
    UserTreeItem* user_item = dynamic_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete user \"%1\"?")
                                  .arg(user_item->text(0)),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        users_.list.erase(users_.list.begin() + user_item->userIndex());

        setConfigChanged(true);
        reloadUserList();
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

void ConfigDialog::onImport()
{
    QString file_path =
        QFileDialog::getOpenFileName(this, tr("Import"), QString(), tr("XML-files (*.xml)"));
    if (file_path.isEmpty())
        return;

    if (Settings::importFromFile(file_path, false, this))
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
        QFileDialog::getSaveFileName(this, tr("Export"), QString(), tr("XML-files (*.xml)"));
    if (file_path.isEmpty())
        return;

    Settings::exportToFile(file_path, false, this);
}

void ConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);

    if (isConfigChanged() && (standard_button == QDialogButtonBox::Ok ||
                              standard_button == QDialogButtonBox::Apply))
    {
        Settings settings;

        if (!settings.isWritable())
        {
            QString message =
                tr("The configuration can not be written. "
                   "Make sure that you have sufficient rights to write.");

            QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
            return;
        }

        settings.setTcpPort(ui.spinbox_port->value());
        settings.setAddFirewallRule(ui.checkbox_add_firewall_rule->isChecked());
        settings.setUserList(users_);
        settings.setUpdateServer(ui.edit_update_server->text());

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

        setConfigChanged(false);
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
    Settings settings;

    users_ = settings.userList();

    reloadServiceStatus();
    reloadUserList();

    ui.spinbox_port->setValue(settings.tcpPort());
    ui.checkbox_add_firewall_rule->setChecked(settings.addFirewallRule());

    ui.checkbox_use_custom_server->setChecked(settings.updateServer() != DEFAULT_UPDATE_SERVER);
    ui.edit_update_server->setText(settings.updateServer());

    ui.edit_update_server->setEnabled(ui.checkbox_use_custom_server->isChecked());

    setConfigChanged(false);
}

void ConfigDialog::reloadUserList()
{
    ui.tree_users->clear();

    for (int i = 0; i < users_.list.size(); ++i)
        ui.tree_users->addTopLevelItem(new UserTreeItem(i, users_.list.at(i)));

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

    if (base::ServiceController::isInstalled(kHostServiceName))
    {
        ui.button_service_install_remove->setText(tr("Remove"));

        base::ServiceController controller = base::ServiceController::open(kHostServiceName);
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
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
    if (controller.isValid())
        return controller.isRunning();

    return false;
}

bool ConfigDialog::installService()
{
    QString service_file_path =
        QApplication::applicationDirPath() + QLatin1Char('/') + QLatin1String(kHostServiceFileName);

    base::ServiceController controller = base::ServiceController::install(
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
        controller.setDependencies(QStringList() << "RpcSs" << "Tcpip" << "NDIS" << "AFD");
        controller.setDescription(kHostServiceDescription);
    }

    return true;
}

bool ConfigDialog::removeService()
{
    if (!base::ServiceController::remove(kHostServiceName))
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
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
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
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
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
