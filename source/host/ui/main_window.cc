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

#include "host/ui/main_window.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/strings/unicode.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "common/ui/status_dialog.h"
#include "common/ui/text_chat_widget.h"
#include "host/user_session_agent.h"
#include "host/user_session_agent_proxy.h"
#include "host/user_session_window_proxy.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/connect_confirm_dialog.h"
#include "host/ui/notifier_window.h"
#include "host/ui/user_settings.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProcess>
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_WIN)
#include "base/win/process_util.h"

#include <QWinEventNotifier>
#include <qt_windows.h>
#include <shellapi.h>
#endif // defined(Q_OS_WIN)

namespace host {

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      window_proxy_(std::make_shared<UserSessionWindowProxy>(
          base::GuiApplication::uiTaskRunner(), this))
{
    LOG(LS_INFO) << "Ctor";

    UserSettings user_settings;
    Application::instance()->setAttribute(
        Qt::AA_DontShowIconsInMenus, !user_settings.showIconsInMenus());

    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    ui.edit_id->setText("-");
    ui.edit_password->setText("-");

    tray_menu_.addAction(ui.action_settings);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(":/img/main.ico"));
    tray_icon_.setContextMenu(&tray_menu_);
    tray_icon_.show();

    updateTrayIconTooltip();

    QTimer* tray_tooltip_timer = new QTimer(this);

    connect(tray_tooltip_timer, &QTimer::timeout, this, &MainWindow::updateTrayIconTooltip);

    tray_tooltip_timer->setInterval(std::chrono::seconds(30));
    tray_tooltip_timer->start();

    uint32_t one_time_sessions = user_settings.oneTimeSessions();

    ui.action_desktop_manage->setChecked(one_time_sessions & proto::SESSION_TYPE_DESKTOP_MANAGE);
    ui.action_desktop_view->setChecked(one_time_sessions & proto::SESSION_TYPE_DESKTOP_VIEW);
    ui.action_file_transfer->setChecked(one_time_sessions & proto::SESSION_TYPE_FILE_TRANSFER);
    ui.action_system_info->setChecked(one_time_sessions & proto::SESSION_TYPE_SYSTEM_INFO);
    ui.action_text_chat->setChecked(one_time_sessions & proto::SESSION_TYPE_TEXT_CHAT);
    ui.action_port_forwarding->setChecked(one_time_sessions & proto::SESSION_TYPE_PORT_FORWARDING);

    connect(ui.menu_access, &QMenu::triggered, this, &MainWindow::onOneTimeSessionsChanged);

    createLanguageMenu(user_settings.locale());
    onSettingsChanged();

    ui.action_show_icons_in_menus->setChecked(user_settings.showIconsInMenus());
    connect(ui.action_show_icons_in_menus, &QAction::triggered, this, [=](bool enable)
    {
        LOG(LS_INFO) << "[ACTION] Show icons in menus changed: " << enable;
        Application* instance = Application::instance();
        instance->setAttribute(Qt::AA_DontShowIconsInMenus, !enable);

        UserSettings user_settings;
        user_settings.setShowIconsInMenus(enable);
    });

    connect(&tray_icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Context)
            return;

        LOG(LS_INFO) << "[ACTION] Tray icon activated";
        onShowHide();
    });

    connect(ui.menu_language, &QMenu::triggered, this, &MainWindow::onLanguageChanged);
    connect(ui.action_settings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(ui.action_show_hide, &QAction::triggered, this, &MainWindow::onShowHide);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::onExit);
    connect(ui.action_help, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);

    setFixedHeight(sizeHint().height());

    connect(ui.button_new_password, &QPushButton::clicked, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] New password";
        if (!agent_proxy_)
        {
            LOG(LS_INFO) << "No agent proxy";
            return;
        }

        agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::NEW_PASSWORD);
    });
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::connectToService()
{
    if (agent_proxy_)
    {
        LOG(LS_INFO) << "Already connected to service";
        agent_proxy_->setOneTimeSessions(calcOneTimeSessions());
        agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::REFRESH);
    }
    else
    {
        agent_proxy_ = std::make_unique<UserSessionAgentProxy>(
            base::GuiApplication::ioTaskRunner(),
            std::make_unique<UserSessionAgent>(window_proxy_));

        LOG(LS_INFO) << "Connecting to service";
        agent_proxy_->start();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::activateHost()
{
    LOG(LS_INFO) << "Activating host";

    show();
    activateWindow();
    connectToService();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::hideToTray()
{
    LOG(LS_INFO) << "Hide application to system tray";

    ui.action_show_hide->setText(tr("Show"));
    setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!should_be_quit_)
    {
        LOG(LS_INFO) << "Ignored close event";

        hideToTray();
        event->ignore();
    }
    else
    {
        window_proxy_->dettach();

        if (notifier_)
        {
            LOG(LS_INFO) << "Close notifier window";
            notifier_->closeNotifier();
        }

        LOG(LS_INFO) << "Application quit";
        QApplication::quit();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onStatusChanged(UserSessionAgent::Status status)
{
    if (status == UserSessionAgent::Status::CONNECTED_TO_SERVICE)
    {
        LOG(LS_INFO) << "The connection to the service was successfully established.";

        if (agent_proxy_)
        {
            agent_proxy_->setOneTimeSessions(calcOneTimeSessions());
        }
        else
        {
            LOG(LS_ERROR) << "No agent proxy";
        }
    }
    else if (status == UserSessionAgent::Status::DISCONNECTED_FROM_SERVICE)
    {
        agent_proxy_.reset();

        LOG(LS_INFO) << "The connection to the service is lost. The application will be closed.";
        realClose();
    }
    else if (status == UserSessionAgent::Status::SERVICE_NOT_AVAILABLE)
    {
        agent_proxy_.reset();

        LOG(LS_INFO) << "The connection to the service has not been established. "
                     << "The application works offline.";
    }
    else
    {
        LOG(LS_ERROR) << "Unandled status code: " << static_cast<int>(status);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!notifier_)
    {
        LOG(LS_INFO) << "Create NotifierWindow";
        notifier_ = new NotifierWindow();

        connect(notifier_, &NotifierWindow::sig_killSession, this, &MainWindow::onKillSession);

        connect(notifier_, &NotifierWindow::sig_voiceChat, this, [this](bool enable)
        {
            if (agent_proxy_)
                agent_proxy_->setVoiceChat(enable);
        });

        connect(notifier_, &NotifierWindow::sig_textChat, this, [=]()
        {
            // TODO
        });

        connect(notifier_, &NotifierWindow::sig_lockMouse, this, [this](bool enable)
        {
            if (agent_proxy_)
            {
                agent_proxy_->setMouseLock(enable);
            }
            else
            {
                LOG(LS_ERROR) << "No agent proxy";
            }
        });

        connect(notifier_, &NotifierWindow::sig_lockKeyboard, this, [this](bool enable)
        {
            if (agent_proxy_)
            {
                agent_proxy_->setKeyboardLock(enable);
            }
            else
            {
                LOG(LS_ERROR) << "No agent proxy";
            }
        });

        connect(notifier_, &NotifierWindow::sig_pause, this, [this](bool enable)
        {
            if (agent_proxy_)
            {
                agent_proxy_->setPause(enable);
            }
            else
            {
                LOG(LS_ERROR) << "No agent proxy";
            }
        });

        notifier_->setAttribute(Qt::WA_DeleteOnClose);
        notifier_->show();
        notifier_->activateWindow();
    }
    else
    {
        LOG(LS_INFO) << "NotifierWindow already exists";
    }

    notifier_->onClientListChanged(clients);

    int text_chat_clients = 0;
    for (const auto& client : clients)
    {
        if (client.session_type == proto::SESSION_TYPE_TEXT_CHAT)
            ++text_chat_clients;
    }

    if (text_chat_clients > 0)
    {
        LOG(LS_INFO) << "Text chat clients: " << text_chat_clients;

        if (text_chat_widget_)
        {
            LOG(LS_INFO) << "Text chat widget already exists";
        }
        else
        {
            LOG(LS_INFO) << "Create text chat widget";

            text_chat_widget_ = new common::TextChatWidget();

            connect(text_chat_widget_, &common::TextChatWidget::sig_sendMessage,
                    this, [this](const proto::TextChatMessage& message)
            {
                if (agent_proxy_)
                {
                    proto::TextChat text_chat;
                    text_chat.mutable_chat_message()->CopyFrom(message);
                    agent_proxy_->onTextChat(text_chat);
                }
                else
                {
                    LOG(LS_ERROR) << "No agent proxy";
                }
            });

            connect(text_chat_widget_, &common::TextChatWidget::sig_sendStatus,
                    this, [this](const proto::TextChatStatus& status)
            {
                if (agent_proxy_)
                {
                    proto::TextChat text_chat;
                    text_chat.mutable_chat_status()->CopyFrom(status);
                    agent_proxy_->onTextChat(text_chat);
                }
                else
                {
                    LOG(LS_ERROR) << "No agent proxy";
                }
            });

            connect(text_chat_widget_, &common::TextChatWidget::sig_textChatClosed, this, [this]()
            {
                std::vector<uint32_t> sessions = notifier_->sessions(proto::SESSION_TYPE_TEXT_CHAT);
                for (const auto& session : sessions)
                    onKillSession(session);
            });

            text_chat_widget_->setAttribute(Qt::WA_DeleteOnClose);
            text_chat_widget_->show();
            text_chat_widget_->activateWindow();
        }
    }
    else
    {
        LOG(LS_INFO) << "No text chat clients";

        if (text_chat_widget_)
        {
            LOG(LS_INFO) << "Close text chat window";
            text_chat_widget_->close();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCredentialsChanged(const proto::internal::Credentials& credentials)
{
    LOG(LS_INFO) << "Credentials changed (host_id=" << credentials.host_id() << ")";

    ui.button_new_password->setEnabled(true);

    bool has_id = credentials.host_id() != base::kInvalidHostId;

    ui.label_icon_id->setEnabled(has_id);
    ui.label_id->setEnabled(has_id);
    ui.edit_id->setEnabled(has_id);
    ui.edit_id->setText(has_id ? QString::number(credentials.host_id()) : tr("Not available"));

    bool has_password = !credentials.password().empty();

    ui.label_icon_password->setEnabled(has_password);
    ui.button_new_password->setEnabled(has_password);
    ui.label_password->setEnabled(has_password);
    ui.edit_password->setEnabled(has_password);
    ui.edit_password->setText(QString::fromStdString(credentials.password()));

    updateTrayIconTooltip();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onRouterStateChanged(const proto::internal::RouterState& state)
{
    LOG(LS_INFO) << "Router state changed (state=" << state.state() << ")";
    last_state_ = state.state();

    QString router;

    if (state.state() != proto::internal::RouterState::DISABLED)
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(base::utf16FromUtf8(state.host_name()));
        address.setPort(static_cast<uint16_t>(state.host_port()));

        router = QString::fromStdU16String(address.toString());
    }

    if (state.state() == proto::internal::RouterState::DISABLED)
        ui.button_status->setEnabled(false);
    else
        ui.button_status->setEnabled(true);

    if (state.state() != proto::internal::RouterState::CONNECTED)
    {
        ui.label_icon_id->setEnabled(false);
        ui.label_icon_password->setEnabled(false);
        ui.button_new_password->setEnabled(false);

        ui.edit_id->setText("-");
        ui.edit_password->setText("-");
    }
    else
    {
        ui.button_status->setEnabled(true);
    }

    QString status;

    switch (state.state())
    {
        case proto::internal::RouterState::DISABLED:
            status = tr("Router is disabled");
            break;

        case proto::internal::RouterState::CONNECTING:
            status = tr("Connecting to a router %1...").arg(router);
            break;

        case proto::internal::RouterState::CONNECTED:
            status = tr("Connected to a router %1").arg(router);
            break;

        case proto::internal::RouterState::FAILED:
            status = tr("Failed to connect to router %1").arg(router);
            break;

        default:
            NOTREACHED();
            return;
    }

    updateStatusBar();

    if (!status_dialog_)
    {
        status_dialog_ = new common::StatusDialog(this);

        connect(ui.button_status, &QPushButton::clicked,
                status_dialog_, &common::StatusDialog::show);
    }

    status_dialog_->addMessage(status);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onConnectConfirmationRequest(
    const proto::internal::ConnectConfirmationRequest& request)
{
    LOG(LS_INFO) << "Connection confirmation request (id=" << request.id() << ")";

    ConnectConfirmDialog dialog(request, this);
    bool accept = dialog.exec() == ConnectConfirmDialog::Accepted;

    LOG(LS_INFO) << "[ACTION] User " << (accept ? "ACCEPT" : "REJECT") << " connection request";

    if (!agent_proxy_)
    {
        LOG(LS_ERROR) << "No agent proxy";
        return;
    }

    agent_proxy_->connectConfirmation(request.id(), accept);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onVideoRecordingStateChanged(
    const std::string& computer_name, const std::string& user_name, bool started)
{
    LOG(LS_INFO) << "Video recoring state changed (user_name=" << user_name
                 << " started=" << started << ")";

    QString message;

    if (started)
    {
        message = tr("User \"%1\" (%2) started screen recording.")
            .arg(QString::fromStdString(user_name), QString::fromStdString(computer_name));
    }
    else
    {
        message = tr("User \"%1\" (%2) stopped screen recording.")
            .arg(QString::fromStdString(user_name), QString::fromStdString(computer_name));
    }

    tray_icon_.showMessage(tr("Aspia Host"), message, QIcon(":/img/main.ico"), 1200);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTextChat(const proto::TextChat& text_chat)
{
    if (text_chat.has_chat_message())
    {
        if (text_chat_widget_)
        {
            text_chat_widget_->readMessage(text_chat.chat_message());

            if (QApplication::applicationState() != Qt::ApplicationActive)
                text_chat_widget_->activateWindow();
        }
    }
    else if (text_chat.has_chat_status())
    {
        if (text_chat_widget_)
            text_chat_widget_->readStatus(text_chat.chat_status());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::realClose()
{
    LOG(LS_INFO) << "realClose called";

    should_be_quit_ = true;
    close();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();

    LOG(LS_INFO) << "[ACTION] Language changed: " << new_locale;

    Application* application = Application::instance();

    UserSettings user_settings;
    user_settings.setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);

    if (notifier_)
        notifier_->retranslateUi();

    if (status_dialog_)
        status_dialog_->retranslateUi();

    updateStatusBar();

    if (agent_proxy_)
    {
        agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::REFRESH);
    }
    else
    {
        LOG(LS_ERROR) << "No agent proxy";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettings()
{
    LOG(LS_INFO) << "[ACTION] Settings";

#if defined(OS_WIN)
    if (!base::win::isProcessElevated())
    {
        LOG(LS_INFO) << "Process not elevated";

        std::filesystem::path current_exec_file;
        if (base::BasePaths::currentExecFile(&current_exec_file))
        {
            SHELLEXECUTEINFOW sei;
            memset(&sei, 0, sizeof(sei));

            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = current_exec_file.c_str();
            sei.hwnd = reinterpret_cast<HWND>(winId());
            sei.nShow = SW_SHOW;
            sei.lpParameters = L"--config";
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;

            if (ShellExecuteExW(&sei))
            {
                QWinEventNotifier* process_watcher = new QWinEventNotifier(this);

                connect(process_watcher, &QWinEventNotifier::activated, this, [this, process_watcher]
                {
                    process_watcher->deleteLater();
                    ui.action_settings->setEnabled(true);
                    onSettingsChanged();
                });

                ui.action_settings->setEnabled(false);

                process_watcher->setHandle(sei.hProcess);
                process_watcher->setEnabled(true);
            }
            else
            {
                PLOG(LS_ERROR) << "ShellExecuteExW failed";
            }
        }
        else
        {
            LOG(LS_ERROR) << "currentExecFile failed";
        }

        return;
    }
#endif

#if defined(OS_LINUX)
    uid_t self_uid = getuid();
    uid_t effective_uid = geteuid();

    if (self_uid != 0 && self_uid == effective_uid)
    {
        LOG(LS_INFO) << "Start settings dialog as super user";

        QProcess* process = new QProcess(this);

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process](int exit_code, QProcess::ExitStatus exit_status)
        {
            LOG(LS_INFO) << "Process finished with exit code: " << exit_code
                         << " (status: " << exit_status << ")";

            process->deleteLater();
            ui.action_settings->setEnabled(true);
            onSettingsChanged();
        });

        ui.action_settings->setEnabled(false);
        process->start("pkexec", QStringList() << "env" << "DISPLAY=:0"
            << QApplication::applicationFilePath() << "--config");
        return;
    }
#endif // defined(OS_LINUX)

    SystemSettings settings;
    if (settings.passwordProtection())
    {
        CheckPasswordDialog dialog(this);
        if (dialog.exec() == CheckPasswordDialog::Accepted)
        {
            ConfigDialog(this).exec();
            onSettingsChanged();
        }
    }
    else
    {
        ConfigDialog(this).exec();
        onSettingsChanged();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onShowHide()
{
    if (isVisible())
    {
        ui.action_show_hide->setText(tr("Show"));
        setVisible(false);
    }
    else
    {
        ui.action_show_hide->setText(tr("Hide"));
        setVisible(true);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onHelp()
{
    LOG(LS_INFO) << "[ACTION] Help";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAbout()
{
    LOG(LS_INFO) << "[ACTION] About";
    common::AboutDialog(tr("Aspia Host"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onExit()
{
    // If the connection to the service is not established, then exit immediately.
    if (!agent_proxy_)
    {
        LOG(LS_INFO) << "No agent proxy";
        realClose();
        return;
    }

    LOG(LS_INFO) << "[ACTION] Exit";

    QMessageBox message_box(QMessageBox::Question,
        tr("Confirmation"),
        tr("If you exit from Aspia, it will not be possible to connect to this computer until "
           "you turn on the computer or Aspia again manually. Do you really want to exit the "
           "application?"),
        QMessageBox::Yes | QMessageBox::No,
        this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(LS_INFO) << "[ACTION] User confirmed exit";
        if (!notifier_)
        {
            LOG(LS_INFO) << "No notifier";
            realClose();
        }
        else
        {
            LOG(LS_INFO) << "Has notifier. Application will be terminated after disconnecting all clients";
            connect(notifier_, &NotifierWindow::sig_finished, this, &MainWindow::realClose);
            notifier_->onStop();
        }
    }
    else
    {
        LOG(LS_INFO) << "[ACTION] User rejected exit";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettingsChanged()
{
    LOG(LS_INFO) << "Settings changed";
    SystemSettings settings;
    ui.action_exit->setEnabled(!settings.isApplicationShutdownDisabled());
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onKillSession(uint32_t session_id)
{
    if (agent_proxy_)
    {
        LOG(LS_INFO) << "Killing session with ID: " << session_id;
        agent_proxy_->killClient(session_id);
    }
    else
    {
        LOG(LS_ERROR) << "No agent proxy";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onOneTimeSessionsChanged()
{
    LOG(LS_INFO) << "[ACTION] One-time sessions changed";

    uint32_t sessions = calcOneTimeSessions();

    UserSettings user_settings;
    user_settings.setOneTimeSessions(sessions);

    if (agent_proxy_)
    {
        agent_proxy_->setOneTimeSessions(sessions);
    }
    else
    {
        LOG(LS_ERROR) << "No agent proxy";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::createLanguageMenu(const QString& current_locale)
{
    Application::LocaleList locale_list = base::GuiApplication::instance()->localeList();
    QActionGroup* language_group = new QActionGroup(this);

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
void MainWindow::updateStatusBar()
{
    LOG(LS_INFO) << "Update status bar";

    QString message;
    QString icon;

    switch (last_state_)
    {
        case proto::internal::RouterState::DISABLED:
            message = tr("Router is disabled");
            icon = ":/img/cross-script.png";
            break;

        case proto::internal::RouterState::CONNECTING:
            message = tr("Connecting to a router...");
            icon = ":/img/arrow-circle-double.png";
            break;

        case proto::internal::RouterState::CONNECTED:
            message = tr("Connected to a router");
            icon = ":/img/tick.png";
            break;

        case proto::internal::RouterState::FAILED:
            message = tr("Connection error");
            icon = ":/img/cross-script.png";
            break;

        default:
            NOTREACHED();
            return;
    }

    ui.button_status->setText(message);
    ui.button_status->setIcon(QIcon(icon));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateTrayIconTooltip()
{
    LOG(LS_INFO) << "Updating tray tooltip";

    QString ip;

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.count(); ++i)
    {
        const QNetworkInterface& interface = interfaces[i];

        QNetworkInterface::InterfaceFlags flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp))
             continue;

        if (!(flags & QNetworkInterface::IsRunning))
            continue;

        QNetworkInterface::InterfaceType type = interface.type();
        if (type != QNetworkInterface::Ethernet && type != QNetworkInterface::Wifi &&
            type != QNetworkInterface::Ieee80211 && type != QNetworkInterface::Ieee80216 &&
            type != QNetworkInterface::Ieee802154)
        {
            continue;
        }

        QList<QNetworkAddressEntry> addresses = interface.addressEntries();
        for (int j = 0; j < addresses.count(); ++j)
        {
            QHostAddress address = addresses[j].ip();
            if (address.isGlobal())
                ip += address.toString() + QLatin1Char('\n');
        }
    }

    if (!ip.isEmpty())
        ip.prepend(tr("IP addresses:") + QLatin1Char('\n'));

    QString tooltip;
    tooltip += tr("Aspia Host") + QLatin1String("\n\n");
    tooltip += tr("ID: %1").arg(ui.edit_id->text()) + QLatin1Char('\n');
    tooltip += ip;

    tray_icon_.setToolTip(tooltip);
}

//--------------------------------------------------------------------------------------------------
uint32_t MainWindow::calcOneTimeSessions()
{
    uint32_t sessions = 0;

    if (ui.action_desktop_manage->isChecked())
        sessions |= proto::SESSION_TYPE_DESKTOP_MANAGE;

    if (ui.action_desktop_view->isChecked())
        sessions |= proto::SESSION_TYPE_DESKTOP_VIEW;

    if (ui.action_file_transfer->isChecked())
        sessions |= proto::SESSION_TYPE_FILE_TRANSFER;

    if (ui.action_system_info->isChecked())
        sessions |= proto::SESSION_TYPE_SYSTEM_INFO;

    if (ui.action_text_chat->isChecked())
        sessions |= proto::SESSION_TYPE_TEXT_CHAT;

    if (ui.action_port_forwarding->isChecked())
        sessions |= proto::SESSION_TYPE_PORT_FORWARDING;

    return sessions;
}

} // namespace host
