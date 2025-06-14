//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/crypto/password_generator.h"
#include "base/desktop/screen_capturer.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "host/ui/change_password_dialog.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/ui/settings_util.h"
#include "host/service_constants.h"
#include "host/system_settings.h"
#include "common/ui/update_dialog.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/service_controller.h"
#include "base/win/windows_version.h"
#endif // defined(Q_OS_WINDOWS)

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>

namespace host {

//--------------------------------------------------------------------------------------------------
ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::StandardButton::Apply);
    if (apply_button)
        apply_button->setText(tr("Apply"));

    //---------------------------------------------------------------------------------------------
    // General Tab
    //---------------------------------------------------------------------------------------------

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    ui.widget_service->setVisible(false);
    ui.groupbox_update_server->setVisible(false);
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

    connect(ui.spinbox_port, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigDialog::onConfigChanged);

    connect(ui.button_service_install_remove, &QPushButton::clicked,
            this, &ConfigDialog::onServiceInstallRemove);
    connect(ui.button_service_start_stop, &QPushButton::clicked,
            this, &ConfigDialog::onServiceStartStop);

    connect(ui.button_import, &QPushButton::clicked, this, &ConfigDialog::onImport);
    connect(ui.button_export, &QPushButton::clicked, this, &ConfigDialog::onExport);

    ui.combobox_update_check_freq->addItem(tr("Once a day"), 1);
    ui.combobox_update_check_freq->addItem(tr("Once a week"), 7);
    ui.combobox_update_check_freq->addItem(tr("Once a month"), 30);

    connect(ui.combobox_update_check_freq, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int /* index */)
    {
        setConfigChanged(FROM_HERE, true);
    });

    connect(ui.checkbox_auto_update, &QCheckBox::toggled, this, [this](bool checked)
    {
        setConfigChanged(FROM_HERE, true);

        ui.label_update_check_freq->setEnabled(checked);
        ui.combobox_update_check_freq->setEnabled(checked);
    });

    connect(ui.checkbox_use_custom_server, &QCheckBox::toggled, this, [this](bool checked)
    {
        setConfigChanged(FROM_HERE, true);

        ui.label_update_server->setEnabled(checked);
        ui.edit_update_server->setEnabled(checked);

        if (!checked)
            ui.edit_update_server->setText(DEFAULT_UPDATE_SERVER);
    });

    connect(ui.edit_update_server, &QLineEdit::textEdited,
            this, &ConfigDialog::onConfigChanged);

    connect(ui.button_check_updates, &QPushButton::clicked, this, [this]()
    {
        common::UpdateDialog(SystemSettings().updateServer(), "host", this).exec();
    });

    //---------------------------------------------------------------------------------------------
    // Security Tab
    //---------------------------------------------------------------------------------------------

    //---------------------------------------------------------------------------------------------
    // Password Protection of Settings

    connect(ui.button_change_password, &QPushButton::clicked,
            this, &ConfigDialog::onChangePassClicked);
    connect(ui.button_pass_protection, &QPushButton::clicked,
            this, &ConfigDialog::onPassProtectClicked);

    //---------------------------------------------------------------------------------------------
    // One-time Password
    connect(ui.checkbox_onetime_password, &QCheckBox::stateChanged,
            this, &ConfigDialog::onOneTimeStateChanged);

    ui.combobox_onetime_pass_change->addItem(tr("On reboot"), 0);
    ui.combobox_onetime_pass_change->addItem(tr("Every 5 minutes"), 5);
    ui.combobox_onetime_pass_change->addItem(tr("Every 30 minutes"), 30);
    ui.combobox_onetime_pass_change->addItem(tr("Every 1 hour"), 60);
    ui.combobox_onetime_pass_change->addItem(tr("Every 6 hours"), 360);
    ui.combobox_onetime_pass_change->addItem(tr("Every 12 hours"), 720);
    ui.combobox_onetime_pass_change->addItem(tr("Every 24 hours"), 1440);

    connect(ui.combobox_onetime_pass_change, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    ui.combobox_onetime_pass_chars->addItem(tr("Letters and digits"),
        base::PasswordGenerator::LOWER_CASE | base::PasswordGenerator::UPPER_CASE |
        base::PasswordGenerator::DIGITS);
    ui.combobox_onetime_pass_chars->addItem(tr("Letters"),
        base::PasswordGenerator::UPPER_CASE | base::PasswordGenerator::LOWER_CASE);
    ui.combobox_onetime_pass_chars->addItem(tr("Digits"),
        base::PasswordGenerator::DIGITS);

    connect(ui.combobox_onetime_pass_chars, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    connect(ui.spinbox_onetime_pass_char_count, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigDialog::onConfigChanged);

    //---------------------------------------------------------------------------------------------
    // Connection Confirmation
    connect(ui.checkbox_conn_confirm_require, &QCheckBox::stateChanged,
            this, &ConfigDialog::onConnConfirmStateChanged);

    ui.combobox_conn_confirm_auto->addItem(tr("Never"), 0);
    ui.combobox_conn_confirm_auto->addItem(tr("15 seconds"), 15);
    ui.combobox_conn_confirm_auto->addItem(tr("30 seconds"), 30);
    ui.combobox_conn_confirm_auto->addItem(tr("45 seconds"), 45);
    ui.combobox_conn_confirm_auto->addItem(tr("60 seconds"), 60);

    connect(ui.combobox_conn_confirm_auto, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    ui.combobox_no_user_action->addItem(tr("Accept connection"),
                                        static_cast<int>(SystemSettings::NoUserAction::ACCEPT));
    ui.combobox_no_user_action->addItem(tr("Reject connection"),
                                        static_cast<int>(SystemSettings::NoUserAction::REJECT));

    connect(ui.combobox_no_user_action, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    //---------------------------------------------------------------------------------------------
    // Other
    connect(ui.checkbox_disable_shutdown, &QCheckBox::toggled, this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    //---------------------------------------------------------------------------------------------
    // Router Tab
    //---------------------------------------------------------------------------------------------

    connect(ui.checkbox_enable_router, &QCheckBox::toggled, this, [this](bool checked)
    {
        setConfigChanged(FROM_HERE, true);
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
    // Advanced Tab
    //---------------------------------------------------------------------------------------------

    ui.combo_video_capturer->addItem(
        tr("Default"), static_cast<quint32>(base::ScreenCapturer::Type::DEFAULT));

#if defined(Q_OS_WINDOWS)
    ui.combo_video_capturer->addItem(
        "DXGI", static_cast<quint32>(base::ScreenCapturer::Type::WIN_DXGI));

    ui.combo_video_capturer->addItem(
        "GDI", static_cast<quint32>(base::ScreenCapturer::Type::WIN_GDI));

    // Mirror screen capture is available only in Windows 7/2008 R2.
    if (base::windowsVersion() == base::VERSION_WIN7)
    {
        ui.combo_video_capturer->addItem(
            "MIRROR", static_cast<quint32>(base::ScreenCapturer::Type::WIN_MIRROR));
    }

#elif defined(Q_OS_LINUX)
    ui.combo_video_capturer->addItem(
        "X11", static_cast<quint32>(base::ScreenCapturer::Type::LINUX_X11));
#elif defined(Q_OS_MACOS)
    ui.combo_video_capturer->addItem(
        "MACOSX", static_cast<quint32>(base::ScreenCapturer::Type::MACOSX));
#else
#error Platform support not implemented
#endif

    connect(ui.combo_video_capturer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]()
    {
        setConfigChanged(FROM_HERE, true);
    });

    ui.tab_bar->setTabVisible(4, QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier));

    //---------------------------------------------------------------------------------------------
    // Other
    //---------------------------------------------------------------------------------------------

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &ConfigDialog::onButtonBoxClicked);
    reloadAll();

    QTimer::singleShot(0, this, &ConfigDialog::adjustSize);
}

//--------------------------------------------------------------------------------------------------
ConfigDialog::~ConfigDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onOneTimeStateChanged(int state)
{
    bool enable = (state == Qt::Checked);

    ui.label_onetime_pass_change->setEnabled(enable);
    ui.combobox_onetime_pass_change->setEnabled(enable);
    ui.label_onetime_pass_chars->setEnabled(enable);
    ui.combobox_onetime_pass_chars->setEnabled(enable);
    ui.label_onetime_pass_char_count->setEnabled(enable);
    ui.spinbox_onetime_pass_char_count->setEnabled(enable);

    setConfigChanged(FROM_HERE, true);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onConnConfirmStateChanged(int state)
{
    bool enable = (state == Qt::Checked);

    ui.label_conn_confirm_auto->setEnabled(enable);
    ui.combobox_conn_confirm_auto->setEnabled(enable);
    ui.label_no_user_action->setEnabled(enable);
    ui.combobox_no_user_action->setEnabled(enable);

    setConfigChanged(FROM_HERE, true);
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onCurrentUserChanged(
    QTreeWidgetItem* /* current */, QTreeWidgetItem* /* previous */)
{
    ui.button_modify->setEnabled(true);
    ui.button_delete->setEnabled(true);

    ui.action_modify->setEnabled(true);
    ui.action_delete->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onAddUser()
{
    LOG(INFO) << "[ACTION] Add user";

    QStringList exist_names;

    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
        exist_names.append(ui.tree_users->topLevelItem(i)->text(0));

    UserDialog dialog(base::User(), exist_names, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        ui.tree_users->addTopLevelItem(new UserTreeItem(dialog.user()));
        setConfigChanged(FROM_HERE, true);
    }
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onModifyUser()
{
    LOG(INFO) << "[ACTION] Modify user";

    UserTreeItem* current_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!current_item)
    {
        LOG(INFO) << "No selected item";
        return;
    }

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
        setConfigChanged(FROM_HERE, true);
    }
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onDeleteUser()
{
    LOG(INFO) << "[ACTION] Delete user";

    UserTreeItem* user_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
    {
        LOG(INFO) << "No selected item";
        return;
    }

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            tr("Are you sure you want to delete user \"%1\"?")
                                .arg(user_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(INFO) << "[ACTION] Accepted by user";
        delete user_item;
        setConfigChanged(FROM_HERE, true);
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
    }

    if (ui.tree_users->topLevelItemCount() <= 0)
    {
        ui.button_modify->setEnabled(false);
        ui.button_delete->setEnabled(false);
    }
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onPassProtectClicked()
{
    SystemSettings settings;

    if (!settings.passwordProtection())
    {
        ChangePasswordDialog dialog(ChangePasswordDialog::Mode::CREATE_NEW_PASSWORD, this);
        if (dialog.exec() == ChangePasswordDialog::Accepted)
        {
            QByteArray hash;
            QByteArray salt;

            if (!SystemSettings::createPasswordHash(dialog.newPassword(), &hash, &salt))
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
            settings.setPasswordHash(QByteArray());
            settings.setPasswordHashSalt(QByteArray());
        }
    }

    QTimer::singleShot(0, this, &ConfigDialog::reloadAll);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onChangePassClicked()
{
    ChangePasswordDialog dialog(ChangePasswordDialog::Mode::CHANGE_PASSWORD, this);
    if (dialog.exec() == ChangePasswordDialog::Accepted)
    {
        QByteArray hash;
        QByteArray salt;

        if (!SystemSettings::createPasswordHash(dialog.newPassword(), &hash, &salt))
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

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onImport()
{
    LOG(INFO) << "[ACTION] Import settings";

    QString file_path =
        QFileDialog::getOpenFileName(this, tr("Import"), QString(), tr("JSON-files (*.json)"));
    if (file_path.isEmpty())
    {
        LOG(INFO) << "No selected file path";
        return;
    }

    if (SettingsUtil::importFromFile(file_path, false, this))
    {
        if (isServiceStarted())
        {
            QString message =
                tr("Service configuration changed. "
                   "For the changes to take effect, you must restart the service. "
                   "Restart the service now?");

            QMessageBox message_box(QMessageBox::Question,
                                    tr("Confirmation"),
                                    message,
                                    QMessageBox::Yes | QMessageBox::No,
                                    this);
            message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
            message_box.button(QMessageBox::No)->setText(tr("No"));

            if (message_box.exec() == QMessageBox::Yes)
            {
                restartService();
            }
        }

        reloadAll();
    }
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onExport()
{
    LOG(INFO) << "[ACTION] Export settings";

    QString file_path =
        QFileDialog::getSaveFileName(this, tr("Export"), QString(), tr("JSON-files (*.json)"));
    if (file_path.isEmpty())
    {
        LOG(INFO) << "No selected file path";
        return;
    }

    SettingsUtil::exportToFile(file_path, false, this);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);

    if (isConfigChanged() && (standard_button == QDialogButtonBox::Ok ||
                              standard_button == QDialogButtonBox::Apply))
    {
        LOG(INFO) << "[ACTION] Accepted by user";

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

            QMessageBox message_box(QMessageBox::Question,
                                    tr("Confirmation"),
                                    message,
                                    QMessageBox::Yes | QMessageBox::No,
                                    this);
            message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
            message_box.button(QMessageBox::No)->setText(tr("No"));

            if (message_box.exec() == QMessageBox::Yes)
            {
                service_restart_required = true;
            }
        }

        settings.setRouterEnabled(ui.checkbox_enable_router->isChecked());
        if (ui.checkbox_enable_router->isChecked())
        {
            base::Address router_address = base::Address::fromString(
                ui.edit_router_address->text(), DEFAULT_ROUTER_TCP_PORT);
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

            QByteArray router_public_key =
                QByteArray::fromHex(ui.edit_router_public_key->toPlainText().toUtf8());
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
        settings.setApplicationShutdownDisabled(ui.checkbox_disable_shutdown->isChecked());
        settings.setTcpPort(static_cast<quint16>(ui.spinbox_port->value()));
        settings.setUserList(*user_list);
        settings.setAutoUpdateEnabled(ui.checkbox_auto_update->isChecked());
        settings.setUpdateCheckFrequency(ui.combobox_update_check_freq->currentData().toInt());
        settings.setUpdateServer(ui.edit_update_server->text());
        settings.setPreferredVideoCapturer(ui.combo_video_capturer->currentData().toUInt());

        settings.setOneTimePassword(ui.checkbox_onetime_password->isChecked());
        settings.setOneTimePasswordExpire(std::chrono::minutes(
            ui.combobox_onetime_pass_change->currentData().toInt()));
        settings.setOneTimePasswordCharacters(
            ui.combobox_onetime_pass_chars->currentData().toUInt());
        settings.setOneTimePasswordLength(ui.spinbox_onetime_pass_char_count->value());

        settings.setConnConfirm(ui.checkbox_conn_confirm_require->isChecked());
        settings.setAutoConnConfirmInterval(std::chrono::seconds(
            ui.combobox_conn_confirm_auto->currentData().toInt()));
        settings.setConnConfirmNoUserAction(static_cast<SystemSettings::NoUserAction>(
            ui.combobox_no_user_action->currentData().toInt()));

        settings.sync();

        setConfigChanged(FROM_HERE, false);

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
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::setConfigChanged(const base::Location& location, bool changed)
{
    LOG(INFO) << "Config changed (from" << location << "changed" << changed << ")";

    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(FATAL) << "Button not found";
        return;
    }

    apply_button->setEnabled(changed);
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::isConfigChanged() const
{
    QPushButton* apply_button = ui.button_box->button(QDialogButtonBox::Apply);
    if (!apply_button)
    {
        DLOG(FATAL) << "Button not found";
        return false;
    }

    return apply_button->isEnabled();
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::reloadAll()
{
    SystemSettings settings;

    ui.checkbox_auto_update->setChecked(settings.isAutoUpdateEnabled());

    int item_index = ui.combobox_update_check_freq->findData(settings.updateCheckFrequency());
    if (item_index != -1)
        ui.combobox_update_check_freq->setCurrentIndex(item_index);

    bool enable_one_time_pass = settings.oneTimePassword();
    ui.checkbox_onetime_password->setChecked(enable_one_time_pass);
    onOneTimeStateChanged(enable_one_time_pass ? Qt::Checked : Qt::Unchecked);

    std::chrono::minutes onetime_pass_change =
        std::chrono::duration_cast<std::chrono::minutes>(settings.oneTimePasswordExpire());
    item_index = ui.combobox_onetime_pass_change->findData(static_cast<int>(onetime_pass_change.count()));
    if (item_index != -1)
        ui.combobox_onetime_pass_change->setCurrentIndex(item_index);

    quint32 onetime_pass_chars = settings.oneTimePasswordCharacters();
    item_index = ui.combobox_onetime_pass_chars->findData(onetime_pass_chars);
    if (item_index != -1)
        ui.combobox_onetime_pass_chars->setCurrentIndex(item_index);

    ui.spinbox_onetime_pass_char_count->setValue(settings.oneTimePasswordLength());

    bool conn_confirm = settings.connConfirm();
    ui.checkbox_conn_confirm_require->setChecked(conn_confirm);
    onConnConfirmStateChanged(conn_confirm ? Qt::Checked : Qt::Unchecked);

    std::chrono::seconds auto_conn_confirm =
        std::chrono::duration_cast<std::chrono::seconds>(settings.autoConnConfirmInterval());
    item_index = ui.combobox_conn_confirm_auto->findData(static_cast<int>(auto_conn_confirm.count()));
    if (item_index != -1)
        ui.combobox_conn_confirm_auto->setCurrentIndex(item_index);

    SystemSettings::NoUserAction no_user_action = settings.connConfirmNoUserAction();
    item_index = ui.combobox_no_user_action->findData(static_cast<int>(no_user_action));
    if (item_index != -1)
        ui.combobox_no_user_action->setCurrentIndex(item_index);

    bool is_router_enabled = settings.isRouterEnabled();

    base::Address router_address(DEFAULT_ROUTER_TCP_PORT);
    router_address.setHost(settings.routerAddress());
    router_address.setPort(settings.routerPort());

    ui.checkbox_enable_router->setChecked(is_router_enabled);
    ui.edit_router_address->setText(router_address.toString());
    ui.edit_router_public_key->setPlainText(
        QString::fromUtf8(settings.routerPublicKey().toHex()));

    ui.label_router_address->setEnabled(is_router_enabled);
    ui.edit_router_address->setEnabled(is_router_enabled);
    ui.label_router_public_key->setEnabled(is_router_enabled);
    ui.edit_router_public_key->setEnabled(is_router_enabled);

    int current_video_capturer =
        ui.combo_video_capturer->findData(settings.preferredVideoCapturer());
    if (current_video_capturer != -1)
        ui.combo_video_capturer->setCurrentIndex(current_video_capturer);

    reloadServiceStatus();
    reloadUserList(*settings.userList());

    ui.spinbox_port->setValue(settings.tcpPort());
    ui.checkbox_use_custom_server->setChecked(settings.updateServer() != DEFAULT_UPDATE_SERVER);
    ui.edit_update_server->setText(settings.updateServer());

    ui.edit_update_server->setEnabled(ui.checkbox_use_custom_server->isChecked());

    if (!settings.passwordProtection())
    {
        ui.button_pass_protection->setText(tr("Install"));
        ui.button_change_password->setVisible(false);
    }
    else
    {
        ui.button_pass_protection->setText(tr("Remove"));
        ui.button_change_password->setVisible(true);
    }

    ui.checkbox_disable_shutdown->setChecked(settings.isApplicationShutdownDisabled());

    setConfigChanged(FROM_HERE, false);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::reloadUserList(const base::UserList& user_list)
{
    ui.tree_users->clear();

    QVector<base::User> list = user_list.list();
    for (const auto& user : std::as_const(list))
        ui.tree_users->addTopLevelItem(new UserTreeItem(user));

    ui.button_modify->setEnabled(false);
    ui.button_delete->setEnabled(false);

    ui.action_modify->setEnabled(false);
    ui.action_delete->setEnabled(false);
}

//--------------------------------------------------------------------------------------------------
void ConfigDialog::reloadServiceStatus()
{
#if defined(Q_OS_WINDOWS)
    ui.button_service_install_remove->setEnabled(true);
    ui.button_service_start_stop->setEnabled(true);

    QString state;

    if (base::ServiceController::isInstalled(kHostServiceName))
    {
        ui.button_service_install_remove->setText(tr("Remove"));

        base::ServiceController controller =
            base::ServiceController::open(kHostServiceName);
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

    ui.label_service_status->setText(tr("Current service state: %1").arg(state));
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::isServiceStarted()
{
#if defined(Q_OS_WINDOWS)
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
    if (controller.isValid())
        return controller.isRunning();
#endif // defined(Q_OS_WINDOWS)
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::installService()
{
#if defined(Q_OS_WINDOWS)
    QString service_file_path = QCoreApplication::applicationDirPath();

    service_file_path.append('/');
    service_file_path.append(kHostServiceFileName);

    base::ServiceController controller = base::ServiceController::install(
        kHostServiceName, kHostServiceDisplayName, service_file_path);
    if (!controller.isValid())
    {
        LOG(INFO) << "Unable to install service";
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The service could not be installed."),
                             QMessageBox::Ok);
        return false;
    }
    else
    {
        controller.setDependencies({ "RpcSs", "Tcpip", "NDIS", "AFD" });
        controller.setDescription(kHostServiceDescription);
    }

    return true;
#else
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::removeService()
{
#if defined(Q_OS_WINDOWS)
    if (!base::ServiceController::remove(kHostServiceName))
    {
        LOG(ERROR) << "Unable to remove service";
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The service could not be removed."),
                             QMessageBox::Ok);
        return false;
    }

    return true;
#else
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::startService()
{
#if defined(Q_OS_WINDOWS)
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
    if (!controller.isValid())
    {
        LOG(ERROR) << "Unable to open service";
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
            LOG(ERROR) << "Unable to start serivce";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be started."),
                                 QMessageBox::Ok);
            return false;
        }
    }

    return true;
#else
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::stopService()
{
#if defined(Q_OS_WINDOWS)
    base::ServiceController controller = base::ServiceController::open(kHostServiceName);
    if (!controller.isValid())
    {
        LOG(ERROR) << "Unable to open service";
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
            LOG(ERROR) << "Unable to stop service";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The service could not be stopped."),
                                 QMessageBox::Ok);
            return false;
        }
    }

    return true;
#else
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool ConfigDialog::restartService()
{
    if (!stopService())
        return false;

    return startService();
}

} // namespace host
