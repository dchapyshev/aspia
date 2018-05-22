//
// PROJECT:         Aspia
// FILE:            host/ui/host_config_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ui/host_config_dialog.h"

#include <QMenu>
#include <QMessageBox>

#include "base/service_controller.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/win/host_service.h"
#include "host/host_settings.h"

namespace aspia {

namespace {

const char kHostServiceFileName[] = "aspia_host_service.exe";

} // namespace

HostConfigDialog::HostConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.pushbutton_service_install_remove, &QPushButton::pressed,
            this, &HostConfigDialog::onServiceInstallRemove);

    connect(ui.pushbutton_service_start_stop, &QPushButton::pressed,
            this, &HostConfigDialog::onServiceStartStop);

    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &HostConfigDialog::onContextMenu);

    connect(ui.tree_users, &QTreeWidget::currentItemChanged,
            this, &HostConfigDialog::onCurrentItemChanged);

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

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &HostConfigDialog::onButtonBoxClicked);

    HostSettings settings;

    user_list_ = settings.userList();

    reloadServiceStatus();
    reloadUserList();

    ui.spinbox_port->setValue(settings.tcpPort());
}

void HostConfigDialog::onServiceInstallRemove()
{
    switch (service_status_)
    {
        case ServiceStatus::NotInstalled:
        {
            HostService host_service;

            QString file_path =
                QCoreApplication::applicationDirPath() + "/" + kHostServiceFileName;

            ServiceController controller =
                ServiceController::install(host_service.serviceName(),
                                           host_service.serviceDisplayName(),
                                           file_path);
            if (controller.isValid())
            {
                controller.setDescription(host_service.serviceDescription());
            }
            else
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The service could not be installed."),
                                     QMessageBox::Ok);
            }
        }
        break;

        case ServiceStatus::Started:
        case ServiceStatus::Stopped:
        {
            HostService host_service;

            ServiceController controller = ServiceController::open(host_service.serviceName());
            if (controller.isValid())
            {
                if (controller.isRunning())
                    controller.stop();

                if (controller.remove())
                    break;
            }

            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be removed."),
                                 QMessageBox::Ok);
        }
        break;

        default:
            break;
    }

    reloadServiceStatus();
}

void HostConfigDialog::onServiceStartStop()
{
    switch (service_status_)
    {
        case ServiceStatus::Started:
        {
            HostService host_service;

            ServiceController controller = ServiceController::open(host_service.serviceName());
            if (!controller.isValid() || !controller.stop())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The service could not be stopped."),
                                     QMessageBox::Ok);
            }
        }
        break;

        case ServiceStatus::Stopped:
        {
            HostService host_service;

            ServiceController controller = ServiceController::open(host_service.serviceName());
            if (!controller.isValid() || !controller.start())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The service could not be started."),
                                     QMessageBox::Ok);
            }
        }
        break;

        default:
            break;
    }

    reloadServiceStatus();
}

void HostConfigDialog::onContextMenu(const QPoint& point)
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
    menu.exec(ui.tree_users->mapToGlobal(point));
}

void HostConfigDialog::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    ui.button_modify->setEnabled(true);
    ui.button_delete->setEnabled(true);

    ui.action_modify->setEnabled(true);
    ui.action_delete->setEnabled(true);
}

void HostConfigDialog::onAddUser()
{
    User user;
    user.setFlags(User::FLAG_ENABLED);

    if (UserDialog(&user_list_, &user, this).exec() == QDialog::Accepted)
    {
        user_list_.push_back(user);
        reloadUserList();
    }
}

void HostConfigDialog::onModifyUser()
{
    UserTreeItem* user_item = reinterpret_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (UserDialog(&user_list_, user_item->user(), this).exec() == QDialog::Accepted)
        reloadUserList();
}

void HostConfigDialog::onDeleteUser()
{
    UserTreeItem* user_item = reinterpret_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete user \"%1\"?")
                                  .arg(user_item->user()->name()),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        user_list_.removeAt(user_item->userIndex());
        reloadUserList();
    }
}

void HostConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);

    if (standard_button == QDialogButtonBox::Ok || standard_button == QDialogButtonBox::Apply)
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

        settings.setTcpPort(ui.spinbox_port->value());
        settings.setUserList(user_list_);

        if (service_status_ == ServiceStatus::Started)
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
                if (!restartService())
                {
                    QMessageBox::warning(this,
                                         tr("Warning"),
                                         tr("Could not restart the service."),
                                         QMessageBox::Ok);
                }
            }
        }
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

void HostConfigDialog::reloadUserList()
{
    for (int i = ui.tree_users->topLevelItemCount() - 1; i >= 0; --i)
        delete ui.tree_users->takeTopLevelItem(i);

    for (int i = 0; i < user_list_.size(); ++i)
        ui.tree_users->addTopLevelItem(new UserTreeItem(i, &user_list_[i]));

    ui.button_modify->setEnabled(false);
    ui.button_delete->setEnabled(false);

    ui.action_modify->setEnabled(false);
    ui.action_delete->setEnabled(false);
}

void HostConfigDialog::reloadServiceStatus()
{
    HostService host_service;

    if (ServiceController::isInstalled(host_service.serviceName()))
    {
        ui.pushbutton_service_install_remove->setText(tr("Remove"));

        ServiceController controller = ServiceController::open(host_service.serviceName());
        if (controller.isValid())
        {
            ui.pushbutton_service_start_stop->setVisible(true);

            if (controller.isRunning())
            {
                service_status_ = ServiceStatus::Started;

                ui.label_service_status->setText(tr("Started"));
                ui.pushbutton_service_start_stop->setText(tr("Stop"));
            }
            else
            {
                service_status_ = ServiceStatus::Stopped;

                ui.label_service_status->setText(tr("Stopped"));
                ui.pushbutton_service_start_stop->setText(tr("Start"));
            }
        }
        else
        {
            service_status_ = ServiceStatus::Unknown;

            ui.label_service_status->setText(tr("Unknown"));
            ui.pushbutton_service_start_stop->setVisible(false);
        }
    }
    else
    {
        service_status_ = ServiceStatus::NotInstalled;

        ui.pushbutton_service_install_remove->setText(tr("Install"));
        ui.label_service_status->setText(tr("Not Installed"));
        ui.pushbutton_service_start_stop->setVisible(false);
    }
}

bool HostConfigDialog::restartService()
{
    HostService host_service;

    ServiceController controller = ServiceController::open(host_service.serviceName());
    if (!controller.isValid())
        return false;

    controller.stop();

    return controller.start();
}

} // namespace aspia
