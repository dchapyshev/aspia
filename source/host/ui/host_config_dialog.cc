//
// PROJECT:         Aspia
// FILE:            host/ui/host_config_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ui/host_config_dialog.h"

#include <QDebug>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QTranslator>

#include "base/service_controller.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/win/host_service.h"
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

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &HostConfigDialog::onButtonBoxClicked);

    user_list_ = settings.userList();

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
    menu.exec(ui.tree_users->mapToGlobal(point));
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
    User user;
    user.setFlags(User::FLAG_ENABLED);

    if (UserDialog(&user_list_, &user, this).exec() == QDialog::Accepted)
    {
        user_list_.push_back(user);
        setConfigChanged(true);
        reloadUserList();
    }
}

void HostConfigDialog::onModifyUser()
{
    UserTreeItem* user_item = dynamic_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (UserDialog(&user_list_, user_item->user(), this).exec() == QDialog::Accepted)
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
                                  .arg(user_item->user()->name()),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        user_list_.removeAt(user_item->userIndex());
        setConfigChanged(true);
        reloadUserList();
    }
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
        settings.setUserList(user_list_);

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
                if (!restartService())
                {
                    QMessageBox::warning(this,
                                         tr("Warning"),
                                         tr("Could not restart the service."),
                                         QMessageBox::Ok);
                }
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
}

void HostConfigDialog::setConfigChanged(bool changed)
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        qFatal("Button not found");
        return;
    }

    apply_button->setEnabled(changed);
}

bool HostConfigDialog::isConfigChanged() const
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        qFatal("Button not found");
        return false;
    }

    return apply_button->isEnabled();
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

bool HostConfigDialog::isServiceStarted()
{
    HostService host_service;

    ServiceController controller = ServiceController::open(host_service.serviceName());
    if (controller.isValid())
        return controller.isRunning();

    return false;
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
