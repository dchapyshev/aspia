//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/net/address.h"
#include "build/build_config.h"
#include "client/router_config_storage.h"
#include "client/ui/application.h"
#include "client/ui/client_settings.h"
#include "client/ui/client_settings_dialog.h"
#include "client/ui/desktop/desktop_config_dialog.h"
#include "client/ui/desktop/qt_desktop_window.h"
#include "client/ui/file_transfer/qt_file_manager_window.h"
#include "client/ui/sys_info/qt_system_info_window.h"
#include "client/ui/text_chat/qt_text_chat_window.h"
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
    ClientSettings& settings = Application::instance()->settings();
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
        Application* instance = Application::instance();
        instance->setAttribute(Qt::AA_DontShowIconsInMenus, !enable);
        instance->settings().setShowIconsInMenus(enable);
    });

    connect(combo_address->lineEdit(), &QLineEdit::returnPressed, this, &ClientWindow::connectToHost);

    connect(ui.menu_language, &QMenu::triggered, this, &ClientWindow::onLanguageChanged);
    connect(ui.action_settings, &QAction::triggered, this, &ClientWindow::onSettings);
    connect(ui.action_help, &QAction::triggered, this, &ClientWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &ClientWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &ClientWindow::close);
    connect(ui.action_clear_history, &QAction::triggered, this, [this]()
    {
        QMessageBox messagebox(this);
        messagebox.setWindowTitle(tr("Confirmation"));
        messagebox.setText(tr("Are you sure you want to clear your connection history?"));
        messagebox.setIcon(QMessageBox::Question);
        messagebox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        messagebox.button(QMessageBox::Yes)->setText(tr("Yes"));
        messagebox.button(QMessageBox::No)->setText(tr("No"));

        if (messagebox.exec() == QMessageBox::Yes)
        {
            ClientSettings& settings = Application::instance()->settings();
            settings.setAddressList(QStringList());
            ui.combo_address->clear();
        }
    });

    connect(ui.combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientWindow::sessionTypeChanged);

    connect(ui.button_session_config, &QPushButton::clicked,
            this, &ClientWindow::sessionConfigButtonPressed);

    connect(ui.button_connect, &QPushButton::clicked, this, &ClientWindow::connectToHost);

#if defined(OS_WIN)
    connect(ui.action_check_for_updates, &QAction::triggered, this, &ClientWindow::onCheckUpdates);
    connect(ui.action_update_settings, &QAction::triggered, this, [this]()
    {
        UpdateSettingsDialog(this).exec();
    });

    if (settings.checkUpdates())
    {
        update_checker_ = std::make_unique<common::UpdateChecker>();

        update_checker_->setUpdateServer(settings.updateServer().toStdString());
        update_checker_->setPackageName("client");

        update_checker_->start(Application::uiTaskRunner(), this);
    }
#else
    ui.action_check_for_updates->setVisible(false);
    ui.action_update_settings->setVisible(false);
#endif

    combo_address->setFocus();
}

//--------------------------------------------------------------------------------------------------
ClientWindow::~ClientWindow() = default;

//--------------------------------------------------------------------------------------------------
void ClientWindow::closeEvent(QCloseEvent* /* event */)
{
    QApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onUpdateCheckedFinished(const base::ByteArray& result)
{
    if (result.empty())
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
            const base::Version& current_version = base::Version::kVersion_CurrentShort;
            const base::Version& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();
                common::UpdateDialog(update_info, this).exec();
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        update_checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();
    client::Application* application = client::Application::instance();

    application->settings().setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);
    reloadSessionTypes();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onSettings()
{
    ClientSettingsDialog(this).exec();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onAbout()
{
    common::AboutDialog(tr("Aspia Client"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->itemData(item_index).toInt());

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

    Application::instance()->settings().setSessionType(session_type);
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::sessionConfigButtonPressed()
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());
    ClientSettings& settings = Application::instance()->settings();

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

        base::Address address = base::Address::fromString(
            current_address.toStdU16String(), DEFAULT_HOST_TCP_PORT);

        if (!address.isValid())
        {
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
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("A host ID was entered, but the router was not configured. "
                                    "You need to configure your router before connecting."),
                                 QMessageBox::Ok);
            return;
        }

        config.address_or_id = current_address.toStdU16String();
    }

    int current_index = combo_address->findText(current_address);
    if (current_index != -1)
        combo_address->removeItem(current_index);

    combo_address->insertItem(0, current_address);
    combo_address->setCurrentIndex(0);

    QStringList address_list;
    for (int i = 0; i < std::min(combo_address->count(), 15); ++i)
        address_list.append(combo_address->itemText(i));

    ClientSettings& settings = Application::instance()->settings();
    settings.setAddressList(address_list);

    if (host_id_entered)
        config.router_config = std::move(router_config);

    config.session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());

    SessionWindow* session_window = nullptr;

    switch (config.session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_window = new QtDesktopWindow(config.session_type, settings.desktopManageConfig());
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_window = new QtDesktopWindow(config.session_type, settings.desktopViewConfig());
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_window = new client::QtFileManagerWindow();
            break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
            session_window = new client::QtSystemInfoWindow();
            break;

        case proto::SESSION_TYPE_TEXT_CHAT:
            session_window = new client::QtTextChatWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!session_window)
        return;

    session_window->setAttribute(Qt::WA_DeleteOnClose);
    if (!session_window->connectToHost(config))
        session_window->close();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onCheckUpdates()
{
#if defined(OS_WIN)
    common::UpdateDialog(Application::instance()->settings().updateServer().toStdString(),
                         "client", this).exec();
#endif
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : qt_base::Application::instance()->localeList())
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
    proto::SessionType current_session_type = Application::instance()->settings().sessionType();
    QComboBox* combobox = ui.combo_session_type;

    auto add_session = [=](const QString& icon, proto::SessionType session_type)
    {
        combobox->addItem(QIcon(icon),
                          common::sessionTypeToLocalizedString(session_type),
                          QVariant(session_type));
    };

    combobox->clear();

    add_session(QStringLiteral(":/img/monitor-keyboard.png"), proto::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(QStringLiteral(":/img/monitor.png"), proto::SESSION_TYPE_DESKTOP_VIEW);
    add_session(QStringLiteral(":/img/folder-stand.png"), proto::SESSION_TYPE_FILE_TRANSFER);
    add_session(QStringLiteral(":/img/computer_info.png"), proto::SESSION_TYPE_SYSTEM_INFO);
    add_session(QStringLiteral(":/img/text-chat.png"), proto::SESSION_TYPE_TEXT_CHAT);

    int item_index = combobox->findData(QVariant(current_session_type));
    if (item_index != -1)
    {
        combobox->setCurrentIndex(item_index);
        sessionTypeChanged(item_index);
    }
}

} // namespace client
