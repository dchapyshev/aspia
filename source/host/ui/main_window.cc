//
// SmartCafe Project
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

#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProcess>
#include <QTimer>
#include <QUrl>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "build/build_config.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "common/ui/status_dialog.h"
#include "common/ui/text_chat_widget.h"
#include "host/user_session_agent.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/connect_confirm_dialog.h"
#include "host/ui/notifier_window.h"
#include "host/ui/user_settings.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/process_util.h"

#include <QWinEventNotifier>
#include <qt_windows.h>
#include <shellapi.h>
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG(INFO) << "Ctor";

    UserSettings user_settings;

    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    ui.edit_id->setText("-");
    ui.edit_password->setText("-");

    tray_menu_.addAction(ui.action_settings);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(":/img/aspia-host.ico"));
    tray_icon_.setContextMenu(&tray_menu_);
    tray_icon_.show();

    updateTrayIconTooltip();

    QTimer* tray_tooltip_timer = new QTimer(this);

    connect(tray_tooltip_timer, &QTimer::timeout, this, &MainWindow::updateTrayIconTooltip);

    tray_tooltip_timer->setInterval(std::chrono::seconds(30));
    tray_tooltip_timer->start();

    quint32 one_time_sessions = user_settings.oneTimeSessions();

    ui.action_desktop_manage->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    ui.action_desktop_view->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_DESKTOP_VIEW);
    ui.action_file_transfer->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_FILE_TRANSFER);
    ui.action_system_info->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_SYSTEM_INFO);
    ui.action_text_chat->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_TEXT_CHAT);
    ui.action_port_forwarding->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_PORT_FORWARDING);

    connect(ui.menu_access, &QMenu::triggered, this, &MainWindow::onOneTimeSessionsChanged);

    createLanguageMenu(user_settings.locale());
    onSettingsChanged();

    connect(&tray_icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Context)
            return;

        LOG(INFO) << "[ACTION] Tray icon activated";
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
        LOG(INFO) << "[ACTION] New password";
        emit sig_updateCredentials(proto::internal::CredentialsRequest::NEW_PASSWORD);
    });

    connect(base::GuiApplication::instance(), &base::GuiApplication::sig_themeChanged,
            this, &MainWindow::onThemeChanged);

    onThemeChanged();
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::connectToService()
{
    if (connected_to_service_)
    {
        LOG(INFO) << "Already connected to service";
        emit sig_oneTimeSessions(calcOneTimeSessions());
        emit sig_updateCredentials(proto::internal::CredentialsRequest::REFRESH);
        return;
    }

    UserSessionAgent* agent = new UserSessionAgent();

    agent->moveToThread(base::GuiApplication::ioThread());

    connect(agent, &UserSessionAgent::sig_statusChanged,
            this, &MainWindow::onStatusChanged,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_clientListChanged,
            this, &MainWindow::onClientListChanged,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_credentialsChanged,
            this, &MainWindow::onCredentialsChanged,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_routerStateChanged,
            this, &MainWindow::onRouterStateChanged,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_connectConfirmationRequest,
            this, &MainWindow::onConnectConfirmationRequest,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_videoRecordingStateChanged,
            this, &MainWindow::onVideoRecordingStateChanged,
            Qt::QueuedConnection);
    connect(agent, &UserSessionAgent::sig_textChat,
            this, &MainWindow::onTextChat,
            Qt::QueuedConnection);

    connect(this, &MainWindow::sig_connectToService,
            agent, &UserSessionAgent::onConnectToService,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_disconnectFromService,
            agent, &UserSessionAgent::deleteLater,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_updateCredentials,
            agent, &UserSessionAgent::onUpdateCredentials,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_oneTimeSessions,
            agent, &UserSessionAgent::onOneTimeSessions,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_killClient,
            agent, &UserSessionAgent::onKillClient,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_connectConfirmation,
            agent, &UserSessionAgent::onConnectConfirmation,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_mouseLock,
            agent, &UserSessionAgent::onMouseLock,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_keyboardLock,
            agent, &UserSessionAgent::onKeyboardLock,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_pause,
            agent, &UserSessionAgent::onPause,
            Qt::QueuedConnection);
    connect(this, &MainWindow::sig_textChat,
            agent, &UserSessionAgent::onTextChat,
            Qt::QueuedConnection);

    LOG(INFO) << "Connecting to service";
    emit sig_connectToService();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::activateHost()
{
    LOG(INFO) << "Activating host";

    show();
    activateWindow();
    connectToService();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::hideToTray()
{
    LOG(INFO) << "Hide application to system tray";

    ui.action_show_hide->setText(tr("Show"));
    setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!should_be_quit_)
    {
        LOG(INFO) << "Ignored close event";

        hideToTray();
        event->ignore();
    }
    else
    {
        if (notifier_)
        {
            LOG(INFO) << "Close notifier window";
            notifier_->closeNotifier();
        }

        LOG(INFO) << "Application quit";
        QApplication::quit();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onStatusChanged(UserSessionAgent::Status status)
{
    if (status == UserSessionAgent::Status::CONNECTED_TO_SERVICE)
    {
        LOG(INFO) << "The connection to the service was successfully established.";
        connected_to_service_ = true;
        emit sig_oneTimeSessions(calcOneTimeSessions());
    }
    else if (status == UserSessionAgent::Status::DISCONNECTED_FROM_SERVICE)
    {
        LOG(INFO) << "The connection to the service is lost. The application will be closed.";
        connected_to_service_ = false;
        emit sig_disconnectFromService();
        realClose();
    }
    else if (status == UserSessionAgent::Status::SERVICE_NOT_AVAILABLE)
    {
        LOG(INFO) << "The connection to the service has not been established. "
                     "The application works offline.";
        connected_to_service_ = false;
        emit sig_disconnectFromService();
    }
    else
    {
        LOG(ERROR) << "Unandled status code: " << static_cast<int>(status);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!notifier_)
    {
        LOG(INFO) << "Create NotifierWindow";
        notifier_ = new NotifierWindow();

        connect(notifier_, &NotifierWindow::sig_killSession, this, &MainWindow::onKillSession);
        connect(notifier_, &NotifierWindow::sig_lockMouse, this, &MainWindow::sig_mouseLock);
        connect(notifier_, &NotifierWindow::sig_lockKeyboard, this, &MainWindow::sig_keyboardLock);
        connect(notifier_, &NotifierWindow::sig_pause, this, &MainWindow::sig_pause);

        notifier_->setAttribute(Qt::WA_DeleteOnClose);
        notifier_->show();
        notifier_->activateWindow();
    }
    else
    {
        LOG(INFO) << "NotifierWindow already exists";
    }

    notifier_->onClientListChanged(clients);

    int text_chat_clients = 0;
    for (const auto& client : clients)
    {
        if (client.session_type == proto::peer::SESSION_TYPE_TEXT_CHAT)
            ++text_chat_clients;
    }

    if (text_chat_clients > 0)
    {
        LOG(INFO) << "Text chat clients:" << text_chat_clients;

        if (text_chat_widget_)
        {
            LOG(INFO) << "Text chat widget already exists";
        }
        else
        {
            LOG(INFO) << "Create text chat widget";

            text_chat_widget_ = new common::TextChatWidget();

            connect(text_chat_widget_, &common::TextChatWidget::sig_sendMessage,
                    this, [this](const proto::text_chat::Message& message)
            {
                proto::text_chat::TextChat text_chat;
                text_chat.mutable_chat_message()->CopyFrom(message);
                emit sig_textChat(text_chat);
            });

            connect(text_chat_widget_, &common::TextChatWidget::sig_sendStatus,
                    this, [this](const proto::text_chat::Status& status)
            {
                proto::text_chat::TextChat text_chat;
                text_chat.mutable_chat_status()->CopyFrom(status);
                emit sig_textChat(text_chat);
            });

            connect(text_chat_widget_, &common::TextChatWidget::sig_textChatClosed, this, [this]()
            {
                QList<quint32> sessions = notifier_->sessions(proto::peer::SESSION_TYPE_TEXT_CHAT);
                for (const auto& session : std::as_const(sessions))
                    onKillSession(session);
            });

            text_chat_widget_->setAttribute(Qt::WA_DeleteOnClose);
            text_chat_widget_->show();
            text_chat_widget_->activateWindow();
        }
    }
    else
    {
        LOG(INFO) << "No text chat clients";

        if (text_chat_widget_)
        {
            LOG(INFO) << "Close text chat window";
            text_chat_widget_->close();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCredentialsChanged(const proto::internal::Credentials& credentials)
{
    LOG(INFO) << "Credentials changed (host_id=" << credentials.host_id() << ")";

    ui.button_new_password->setEnabled(true);

    bool has_id = credentials.host_id() != base::kInvalidHostId;

    ui.edit_id->setEnabled(has_id);
    ui.edit_id->setText(has_id ? QString::number(credentials.host_id()) : tr("Not available"));

    bool has_password = !credentials.password().empty();

    ui.button_new_password->setEnabled(has_password);
    ui.edit_password->setEnabled(has_password);
    ui.edit_password->setText(QString::fromStdString(credentials.password()));

    updateTrayIconTooltip();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onRouterStateChanged(const proto::internal::RouterState& state)
{
    LOG(INFO) << "Router state changed (state=" << state.state() << ")";
    last_state_ = state.state();

    QString router;

    if (state.state() != proto::internal::RouterState::DISABLED)
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(QString::fromStdString(state.host_name()));
        address.setPort(static_cast<quint16>(state.host_port()));

        router = address.toString();
    }

    if (state.state() == proto::internal::RouterState::DISABLED)
        ui.button_status->setEnabled(false);
    else
        ui.button_status->setEnabled(true);

    if (state.state() != proto::internal::RouterState::CONNECTED)
    {
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
    LOG(INFO) << "Connection confirmation request (id=" << request.id() << ")";

    ConnectConfirmDialog dialog(request, this);
    bool accept = dialog.exec() == ConnectConfirmDialog::Accepted;

    LOG(INFO) << "[ACTION] User" << (accept ? "ACCEPT" : "REJECT") << "connection request";
    emit sig_connectConfirmation(request.id(), accept);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onVideoRecordingStateChanged(
    const QString& computer_name, const QString& user_name, bool started)
{
    LOG(INFO) << "Video recoring state changed (user_name=" << user_name
              << "started=" << started << ")";

    QString message;

    if (started)
        message = tr("User \"%1\" (%2) started screen recording.").arg(user_name, computer_name);
    else
        message = tr("User \"%1\" (%2) stopped screen recording.").arg(user_name, computer_name);

    tray_icon_.showMessage(tr("SmartCafe Host"), message, QIcon(":/img/aspia-host.ico"), 1200);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTextChat(const proto::text_chat::TextChat& text_chat)
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
        LOG(ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::realClose()
{
    LOG(INFO) << "realClose called";

    emit sig_disconnectFromService();
    should_be_quit_ = true;
    close();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();

    LOG(INFO) << "[ACTION] Language changed:" << new_locale;

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
    emit sig_updateCredentials(proto::internal::CredentialsRequest::REFRESH);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onThemeChanged()
{
    QTimer::singleShot(100, this, [this]()
    {
        auto set_edit_colors = [](QLineEdit* edit, const QString& color)
        {
            edit->setStyleSheet(QString("QLineEdit {"
                                            "border: 0;"
                                            "background-color: %1;"
                                            "font-size: 20px;"
                                            "padding-left: 20px;"
                                        "}").arg(color));
        };

        static const QString kLabelStyle = QStringLiteral("QLabel {"
                                                              "font: bold 11px;"
                                                              "padding-left: 3px;"
                                                          "}");

        QPalette window_palette = palette();
        QString edit_color = window_palette.color(QPalette::Window).name(QColor::HexRgb);
        QString window_text = window_palette.color(QPalette::WindowText).name(QColor::HexRgb);

        ui.label_id->setStyleSheet(kLabelStyle);
        ui.label_password->setStyleSheet(kLabelStyle);

        set_edit_colors(ui.edit_id, edit_color);
        set_edit_colors(ui.edit_password, edit_color);

        ui.button_status->setStyleSheet("QPushButton {"
                                            "font: 11px;"
                                            "text-align: left;"
                                            "border: 0;"
                                        "}"
                                        "QPushButton:hover:enabled {"
                                            "text-decoration: underline;"
                                        "}");

        updateStatusBar();
    });
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettings()
{
    LOG(INFO) << "[ACTION] Settings";

#if defined(Q_OS_WINDOWS)
    if (!base::isProcessElevated())
    {
        LOG(INFO) << "Process not elevated";

        QString current_exec_file =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        if (!current_exec_file.isEmpty())
        {
            SHELLEXECUTEINFOW sei;
            memset(&sei, 0, sizeof(sei));

            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = qUtf16Printable(current_exec_file);
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
                PLOG(ERROR) << "ShellExecuteExW failed";
            }
        }
        else
        {
            LOG(ERROR) << "Empty file path";
        }

        return;
    }
#endif

#if defined(Q_OS_LINUX)
    uid_t self_uid = getuid();
    uid_t effective_uid = geteuid();

    if (self_uid != 0 && self_uid == effective_uid)
    {
        LOG(INFO) << "Start settings dialog as super user";

        QProcess* process = new QProcess(this);

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process](int exit_code, QProcess::ExitStatus exit_status)
        {
            LOG(INFO) << "Process finished with exit code:" << exit_code
                      << "(status:" << exit_status << ")";

            process->deleteLater();
            ui.action_settings->setEnabled(true);
            onSettingsChanged();
        });

        ui.action_settings->setEnabled(false);
        process->start("pkexec", QStringList() << "env" << "DISPLAY=:0"
            << QApplication::applicationFilePath() << "--config");
        return;
    }
#endif // defined(Q_OS_LINUX)

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
    LOG(INFO) << "[ACTION] Help";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAbout()
{
    LOG(INFO) << "[ACTION] About";
    common::AboutDialog(tr("SmartCafe Host"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onExit()
{
    // If the connection to the service is not established, then exit immediately.
    if (!connected_to_service_)
    {
        LOG(INFO) << "No agent proxy";
        realClose();
        return;
    }

    LOG(INFO) << "[ACTION] Exit";

    QMessageBox message_box(QMessageBox::Question,
        tr("Confirmation"),
        tr("If you exit from SmartCafe, it will not be possible to connect to this computer until "
           "you turn on the computer or SmartCafe again manually. Do you really want to exit the "
           "application?"),
        QMessageBox::Yes | QMessageBox::No,
        this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(INFO) << "[ACTION] User confirmed exit";
        if (!notifier_)
        {
            LOG(INFO) << "No notifier";
            realClose();
        }
        else
        {
            LOG(INFO) << "Has notifier. Application will be terminated after disconnecting all clients";
            connect(notifier_, &NotifierWindow::sig_finished, this, &MainWindow::realClose);
            notifier_->onStop();
        }
    }
    else
    {
        LOG(INFO) << "[ACTION] User rejected exit";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettingsChanged()
{
    LOG(INFO) << "Settings changed";
    SystemSettings settings;
    ui.action_exit->setEnabled(!settings.isApplicationShutdownDisabled());
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onKillSession(quint32 session_id)
{
    LOG(INFO) << "Killing session with ID:" << session_id;
    emit sig_killClient(session_id);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onOneTimeSessionsChanged()
{
    LOG(INFO) << "[ACTION] One-time sessions changed";

    quint32 sessions = calcOneTimeSessions();

    UserSettings user_settings;
    user_settings.setOneTimeSessions(sessions);

    emit sig_oneTimeSessions(sessions);
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
    LOG(INFO) << "Update status bar";

    QString message;
    QString icon;

    switch (last_state_)
    {
        case proto::internal::RouterState::DISABLED:
            message = tr("Router is disabled");
            icon = ":/img/close.svg";
            break;

        case proto::internal::RouterState::CONNECTING:
            message = tr("Connecting to a router...");
            icon = ":/img/replay.svg";
            break;

        case proto::internal::RouterState::CONNECTED:
            message = tr("Connected to a router");
            icon = ":/img/done.svg";
            break;

        case proto::internal::RouterState::FAILED:
            message = tr("Connection error");
            icon = ":/img/close.svg";
            break;

        default:
            NOTREACHED();
            return;
    }

    ui.button_status->setText(message);
    ui.button_status->setIcon(QIcon(icon));

    QString statusbar_text_color = palette().color(QPalette::WindowText).name(QColor::HexRgb);

    ui.button_status->setStyleSheet(QString("QPushButton {"
                                        "color: %1;"
                                        "font: 11px;"
                                        "text-align: left;"
                                        "border: 0;"
                                    "}"
                                    "QPushButton:hover:enabled {"
                                        "text-decoration: underline;"
                                    "}").arg(statusbar_text_color));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateTrayIconTooltip()
{
    LOG(INFO) << "Updating tray tooltip";

    QString ip;

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.count(); ++i)
    {
        const QNetworkInterface& iface = interfaces[i];

        QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp))
             continue;

        if (!(flags & QNetworkInterface::IsRunning))
            continue;

        QNetworkInterface::InterfaceType type = iface.type();
        if (type != QNetworkInterface::Ethernet && type != QNetworkInterface::Wifi &&
            type != QNetworkInterface::Ieee80211 && type != QNetworkInterface::Ieee80216 &&
            type != QNetworkInterface::Ieee802154)
        {
            continue;
        }

        QList<QNetworkAddressEntry> addresses = iface.addressEntries();
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
    tooltip += tr("SmartCafe Host") + QLatin1String("\n\n");
    tooltip += tr("ID: %1").arg(ui.edit_id->text()) + QLatin1Char('\n');
    tooltip += ip;

    tray_icon_.setToolTip(tooltip);
}

//--------------------------------------------------------------------------------------------------
quint32 MainWindow::calcOneTimeSessions()
{
    quint32 sessions = 0;

    if (ui.action_desktop_manage->isChecked())
        sessions |= proto::peer::SESSION_TYPE_DESKTOP_MANAGE;

    if (ui.action_desktop_view->isChecked())
        sessions |= proto::peer::SESSION_TYPE_DESKTOP_VIEW;

    if (ui.action_file_transfer->isChecked())
        sessions |= proto::peer::SESSION_TYPE_FILE_TRANSFER;

    if (ui.action_system_info->isChecked())
        sessions |= proto::peer::SESSION_TYPE_SYSTEM_INFO;

    if (ui.action_text_chat->isChecked())
        sessions |= proto::peer::SESSION_TYPE_TEXT_CHAT;

    if (ui.action_port_forwarding->isChecked())
        sessions |= proto::peer::SESSION_TYPE_PORT_FORWARDING;

    return sessions;
}

} // namespace host
