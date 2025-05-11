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

#include "client/ui/client_window.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "client/router_config_storage.h"
#include "client/ui/application.h"
#include "client/ui/client_settings.h"
#include "client/ui/client_settings_dialog.h"
#include "client/ui/desktop/desktop_config_dialog.h"
#include "client/ui/desktop/desktop_session_window.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"
#include "client/ui/text_chat/text_chat_session_window.h"
#include "client/ui/update_settings_dialog.h"
#include "common/desktop_session_constants.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "common/ui/session_type.h"
#include "common/ui/update_dialog.h"

#include <QActionGroup>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace client {

//--------------------------------------------------------------------------------------------------
ClientWindow::ClientWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG(LS_INFO) << "Ctor";

    ClientSettings settings;
    Application::instance()->setAttribute(
        Qt::AA_DontShowIconsInMenus, !settings.showIconsInMenus());

    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    createLanguageMenu(settings.locale());
    reloadSessionTypes();

    QComboBox* combo_address = ui.combo_address;

    combo_address->addItems(settings.addressList());
    combo_address->setCurrentIndex(0);

    ui.action_show_icons_in_menus->setChecked(settings.showIconsInMenus());
    connect(ui.action_show_icons_in_menus, &QAction::triggered, this, [=](bool enable)
    {
        LOG(LS_INFO) << "[ACTION] Show icons in menus: " << enable;

        Application* instance = Application::instance();
        instance->setAttribute(Qt::AA_DontShowIconsInMenus, !enable);

        ClientSettings settings;
        settings.setShowIconsInMenus(enable);
    });

    connect(combo_address->lineEdit(), &QLineEdit::returnPressed, this, &ClientWindow::connectToHost);

    connect(ui.menu_language, &QMenu::triggered, this, &ClientWindow::onLanguageChanged);
    connect(ui.action_settings, &QAction::triggered, this, &ClientWindow::onSettings);
    connect(ui.action_help, &QAction::triggered, this, &ClientWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &ClientWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &ClientWindow::close);
    connect(ui.action_clear_history, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Clear history";

        QMessageBox messagebox(this);
        messagebox.setWindowTitle(tr("Confirmation"));
        messagebox.setText(tr("Are you sure you want to clear your connection history?"));
        messagebox.setIcon(QMessageBox::Question);
        messagebox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        messagebox.button(QMessageBox::Yes)->setText(tr("Yes"));
        messagebox.button(QMessageBox::No)->setText(tr("No"));

        if (messagebox.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Accepted by user";

            ClientSettings settings;
            settings.setAddressList(QStringList());
            ui.combo_address->clear();
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Rejected by user";
        }
    });

    connect(ui.combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientWindow::sessionTypeChanged);

    connect(ui.button_session_config, &QPushButton::clicked,
            this, &ClientWindow::sessionConfigButtonPressed);

    connect(ui.button_connect, &QPushButton::clicked, this, &ClientWindow::connectToHost);

#if defined(Q_OS_WINDOWS)
    connect(ui.action_check_for_updates, &QAction::triggered, this, &ClientWindow::onCheckUpdates);
    connect(ui.action_update_settings, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Update settings dialog";
        UpdateSettingsDialog(this).exec();
    });

    if (settings.checkUpdates())
    {
        update_checker_ = std::make_unique<common::UpdateChecker>();

        update_checker_->setUpdateServer(settings.updateServer());
        update_checker_->setPackageName("client");

        connect(update_checker_.get(), &common::UpdateChecker::sig_checkedFinished,
                this, &ClientWindow::onUpdateCheckedFinished);

        LOG(LS_INFO) << "Start update checker";
        update_checker_->start();
    }
#else
    ui.action_check_for_updates->setVisible(false);
    ui.action_update_settings->setVisible(false);
#endif

    combo_address->setFocus();
}

//--------------------------------------------------------------------------------------------------
ClientWindow::~ClientWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(LS_INFO) << "Close event detected";
    QApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(LS_ERROR) << "Error while retrieving update information";
    }
    else
    {
        common::UpdateInfo update_info = common::UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(LS_INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = base::kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();
                common::UpdateDialog(update_info, this).exec();
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        LOG(LS_INFO) << "Destroy update checker";
        update_checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();
    client::Application* application = client::Application::instance();

    LOG(LS_INFO) << "[ACTION] Language changed: " << new_locale.toStdString();

    ClientSettings settings;
    settings.setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);
    reloadSessionTypes();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onSettings()
{
    LOG(LS_INFO) << "[ACTION] Settings button";
    ClientSettingsDialog(this).exec();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onHelp()
{
    LOG(LS_INFO) << "[ACTION] Help button";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onAbout()
{
    LOG(LS_INFO) << "[ACTION] About button";
    common::AboutDialog(tr("Aspia Client"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->itemData(item_index).toInt());

    LOG(LS_INFO) << "[ACTION] Session type changed: " << session_type;

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui.button_session_config->setEnabled(true);
            break;

        default:
            ui.button_session_config->setEnabled(false);
            break;
    }

    ClientSettings settings;
    settings.setSessionType(session_type);
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::sessionConfigButtonPressed()
{
    LOG(LS_INFO) << "[ACTION] Session config button";

    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());
    ClientSettings settings;

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            DesktopConfigDialog dialog(session_type,
                                       settings.desktopManageConfig(),
                                       common::kSupportedVideoEncodings,
                                       this);

            if (dialog.exec() == DesktopConfigDialog::Accepted)
                settings.setDesktopManageConfig(dialog.config());
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            DesktopConfigDialog dialog(session_type,
                                       settings.desktopViewConfig(),
                                       common::kSupportedVideoEncodings,
                                       this);

            if (dialog.exec() == DesktopConfigDialog::Accepted)
                settings.setDesktopViewConfig(dialog.config());
        }
        break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::connectToHost()
{
    LOG(LS_INFO) << "[ACTION] Connect to host";

    RouterConfig router_config = RouterConfigStorage().routerConfig();
    Config config;

    QComboBox* combo_address = ui.combo_address;
    QString current_address = combo_address->currentText();

    bool host_id_entered = true;

    for (int i = 0; i < current_address.length(); ++i)
    {
        if (!current_address[i].isDigit())
        {
            host_id_entered = false;
            break;
        }
    }

    if (!host_id_entered)
    {
        LOG(LS_INFO) << "Direct connection selected";

        base::Address address = base::Address::fromString(current_address, DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            LOG(LS_ERROR) << "Invalid computer address";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("An invalid computer address was entered."),
                                 QMessageBox::Ok);
            combo_address->setFocus();
            return;
        }

        config.address_or_id = address.host();
        config.port = address.port();
    }
    else
    {
        LOG(LS_INFO) << "Relay connection selected";

        if (!router_config.isValid())
        {
            LOG(LS_ERROR) << "Router not configured";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("A host ID was entered, but the router was not configured. "
                                    "You need to configure your router before connecting."),
                                 QMessageBox::Ok);
            return;
        }

        config.address_or_id = current_address;
    }

    int current_index = combo_address->findText(current_address);
    if (current_index != -1)
        combo_address->removeItem(current_index);

    combo_address->insertItem(0, current_address);
    combo_address->setCurrentIndex(0);

    QStringList address_list;
    for (int i = 0; i < std::min(combo_address->count(), 15); ++i)
        address_list.append(combo_address->itemText(i));

    ClientSettings settings;
    settings.setAddressList(address_list);

    if (host_id_entered)
        config.router_config = std::move(router_config);

    config.session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());
    config.display_name = settings.displayName();

    SessionWindow* session_window = nullptr;

    switch (config.session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_window = new DesktopSessionWindow(config.session_type, settings.desktopManageConfig());
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_window = new DesktopSessionWindow(config.session_type, settings.desktopViewConfig());
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_window = new FileTransferSessionWindow();
            break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
            session_window = new SystemInfoSessionWindow();
            break;

        case proto::SESSION_TYPE_TEXT_CHAT:
            session_window = new TextChatSessionWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!session_window)
    {
        LOG(LS_ERROR) << "Session window not created";
        return;
    }

    session_window->setAttribute(Qt::WA_DeleteOnClose);
    if (!session_window->connectToHost(config))
    {
        LOG(LS_ERROR) << "Unable to connect to host";
        session_window->close();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onCheckUpdates()
{
    LOG(LS_INFO) << "[ACTION] Check updates";
#if defined(Q_OS_WINDOWS)
    ClientSettings settings;
    common::UpdateDialog(settings.updateServer(), "client", this).exec();
#endif
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);
    Application::LocaleList locale_list = base::GuiApplication::instance()->localeList();

    for (const auto& locale : std::as_const(locale_list))
    {
        common::LanguageAction* action_language =
            new common::LanguageAction(locale.first, locale.second, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale.first)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::reloadSessionTypes()
{
    ClientSettings settings;
    proto::SessionType current_session_type = settings.sessionType();
    QComboBox* combobox = ui.combo_session_type;

    auto add_session = [=](const QString& icon, proto::SessionType session_type)
    {
        combobox->addItem(QIcon(icon),
                          common::sessionTypeToLocalizedString(session_type),
                          QVariant(session_type));
    };

    combobox->clear();

    add_session(":/img/monitor-keyboard.png", proto::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(":/img/monitor.png", proto::SESSION_TYPE_DESKTOP_VIEW);
    add_session(":/img/folder-stand.png", proto::SESSION_TYPE_FILE_TRANSFER);
    add_session(":/img/computer_info.png", proto::SESSION_TYPE_SYSTEM_INFO);
    add_session(":/img/text-chat.png", proto::SESSION_TYPE_TEXT_CHAT);

    int item_index = combobox->findData(QVariant(current_session_type));
    if (item_index != -1)
    {
        combobox->setCurrentIndex(item_index);
        sessionTypeChanged(item_index);
    }
}

} // namespace client
