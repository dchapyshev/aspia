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

#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QTranslator>

#include "base/logging.h"
#include "base/service_controller.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/win/host_service_constants.h"
#include "host/host_settings.h"

namespace aspia {

HostConfigDialog::HostConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    HostSettings settings;

    QString current_locale = settings.locale();

    if (!locale_loader_.contains(current_locale))
    {
        current_locale = HostSettings::defaultLocale();
        settings.setLocale(current_locale);
    }

    locale_loader_.installTranslators(current_locale);
    ui.setupUi(this);
    createLanguageList(current_locale);

    connect(ui.combobox_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int /* index */)
    {
        setConfigChanged(true);
    });

    connect(ui.spinbox_port, QOverload<int>::of(&QSpinBox::valueChanged), [this](int /* value */)
    {
        setConfigChanged(true);
    });

    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &HostConfigDialog::onUserContextMenu);

    connect(ui.tree_users, &QTreeWidget::currentItemChanged,
            this, &HostConfigDialog::onCurrentUserChanged);

    connect(ui.tree_users, &QTreeWidget::itemDoubleClicked,
            [this](QTreeWidgetItem* /* item */, int /* column */)
    {
        onModifyUser();
    });

    connect(ui.action_add, &QAction::triggered, this, &HostConfigDialog::onAddUser);
    connect(ui.action_modify, &QAction::triggered, this, &HostConfigDialog::onModifyUser);
    connect(ui.action_delete, &QAction::triggered, this, &HostConfigDialog::onDeleteUser);
    connect(ui.button_add, &QPushButton::pressed, this, &HostConfigDialog::onAddUser);
    connect(ui.button_modify, &QPushButton::pressed, this, &HostConfigDialog::onModifyUser);
    connect(ui.button_delete, &QPushButton::pressed, this, &HostConfigDialog::onDeleteUser);

    connect(ui.button_service_install_remove, &QPushButton::pressed,
            this, &HostConfigDialog::onServiceInstallRemove);
    connect(ui.button_service_start_stop, &QPushButton::pressed,
            this, &HostConfigDialog::onServiceStartStop);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &HostConfigDialog::onButtonBoxClicked);

    user_list_ = settings.userList();

    reloadServiceStatus();
    reloadUserList();

    ui.spinbox_port->setValue(settings.tcpPort());

    setConfigChanged(false);
}

void HostConfigDialog::onUserContextMenu(const QPoint& point)
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

void HostConfigDialog::onCurrentUserChanged(
    QTreeWidgetItem* /* current */, QTreeWidgetItem* /* previous */)
{
    ui.button_modify->setEnabled(true);
    ui.button_delete->setEnabled(true);

    ui.action_modify->setEnabled(true);
    ui.action_delete->setEnabled(true);
}

void HostConfigDialog::onAddUser()
{
    proto::SrpUser user;
    user.set_flags(proto::SrpUser::ENABLED);

    if (UserDialog(*user_list_, &user, this).exec() == QDialog::Accepted)
    {
        user_list_->add_user()->Swap(&user);
        setConfigChanged(true);
        reloadUserList();
    }
}

void HostConfigDialog::onModifyUser()
{
    UserTreeItem* user_item = dynamic_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (UserDialog(*user_list_, user_item->user(), this).exec() == QDialog::Accepted)
    {
        setConfigChanged(true);
        reloadUserList();
    }
}

void HostConfigDialog::onDeleteUser()
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
        for (int i = 0; i < user_list_->user_size(); ++i)
        {
            if (user_list_->mutable_user(i) == user_item->user())
            {
                user_list_->mutable_user()->DeleteSubrange(i, 1);
                break;
            }
        }

        setConfigChanged(true);
        reloadUserList();
    }
}

void HostConfigDialog::onServiceInstallRemove()
{
    switch (service_state_)
    {
        case ServiceState::NOT_INSTALLED:
            installService();
            break;

        case ServiceState::NOT_STARTED:
        case ServiceState::STARTED:
        {
            if (service_state_ == ServiceState::STARTED && !stopService())
                break;

            removeService();
        }
        break;

        default:
            break;
    }

    reloadServiceStatus();
}

void HostConfigDialog::onServiceStartStop()
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

void HostConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);

    if (isConfigChanged() && (standard_button == QDialogButtonBox::Ok ||
                              standard_button == QDialogButtonBox::Apply))
    {
        HostSettings settings;

        if (!settings.isWritable())
        {
            QString message =
                tr("The configuration can not be written. "
                   "Make sure that you have sufficient rights to write.");

            QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
            return;
        }

        QString new_locale = ui.combobox_language->currentData().toString();

        if (standard_button == QDialogButtonBox::Apply)
            retranslateUi(new_locale);

        settings.setLocale(new_locale);
        settings.setTcpPort(ui.spinbox_port->value());
        settings.setUserList(*user_list_);

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

void HostConfigDialog::createLanguageList(const QString& current_locale)
{
    for (const auto& locale : locale_loader_.sortedLocaleList())
    {
        const QString language = QLocale::languageToString(QLocale(locale).language());

        ui.combobox_language->addItem(language, locale);
        if (current_locale == locale)
            ui.combobox_language->setCurrentText(language);
    }
}

void HostConfigDialog::retranslateUi(const QString& locale)
{
    locale_loader_.installTranslators(locale);
    ui.retranslateUi(this);
    reloadServiceStatus();
}

void HostConfigDialog::setConfigChanged(bool changed)
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(LS_FATAL) << "Button not found";
        return;
    }

    apply_button->setEnabled(changed);
}

bool HostConfigDialog::isConfigChanged() const
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(LS_FATAL) << "Button not found";
        return false;
    }

    return apply_button->isEnabled();
}

void HostConfigDialog::reloadUserList()
{
    for (int i = ui.tree_users->topLevelItemCount() - 1; i >= 0; --i)
        delete ui.tree_users->takeTopLevelItem(i);

    for (int i = 0; i < user_list_->user_size(); ++i)
        ui.tree_users->addTopLevelItem(new UserTreeItem(user_list_->mutable_user(i)));

    ui.button_modify->setEnabled(false);
    ui.button_delete->setEnabled(false);

    ui.action_modify->setEnabled(false);
    ui.action_delete->setEnabled(false);
}

void HostConfigDialog::reloadServiceStatus()
{
    ui.button_service_install_remove->setEnabled(true);
    ui.button_service_start_stop->setEnabled(true);

    QString state;

    if (ServiceController::isInstalled(kHostServiceName))
    {
        ui.button_service_install_remove->setText(tr("Remove"));

        ServiceController controller = ServiceController::open(kHostServiceName);
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

bool HostConfigDialog::isServiceStarted()
{
    ServiceController controller = ServiceController::open(kHostServiceName);
    if (controller.isValid())
        return controller.isRunning();

    return false;
}

bool HostConfigDialog::installService()
{
    QString service_file_path =
        QApplication::applicationDirPath() + QLatin1Char('/') + QLatin1String(kHostServiceFileName);

    ServiceController controller =
        ServiceController::install(kHostServiceName, kHostServiceDisplayName, service_file_path);
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
        controller.setDescription(kHostServiceDescription);
    }

    return true;
}

bool HostConfigDialog::removeService()
{
    ServiceController controller = ServiceController::open(kHostServiceName);
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
        if (!controller.remove())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be removed."),
                                 QMessageBox::Ok);
            return false;
        }
    }

    return true;
}

bool HostConfigDialog::startService()
{
    ServiceController controller = ServiceController::open(kHostServiceName);
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

bool HostConfigDialog::stopService()
{
    ServiceController controller = ServiceController::open(kHostServiceName);
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

bool HostConfigDialog::restartService()
{
    if (!stopService())
        return false;

    return startService();
}

} // namespace aspia
