//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/host_window.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QMenuBar>
#include <QNetworkInterface>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "build/build_config.h"
#include "common/clipboard.h"
#include "common/desktop/about_dialog.h"
#include "common/desktop/chat_widget.h"
#include "common/desktop/language_action.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/status_dialog.h"
#include "host/database.h"
#include "host/system_settings.h"
#include "host/user_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/connect_confirm_dialog.h"
#include "host/ui/elevate_util.h"
#include "host/ui/notifier_window.h"
#include "host/ui/security_log_dialog.h"
#include "proto/desktop_clipboard.h"
#include "proto/user.h"
#include "ui_host_window.h"

#if defined(Q_OS_MACOS)
#include "host/ui/permission_dialog.h"
#endif // defined(Q_OS_MACOS)

namespace {

// How many one-second tray availability checks to make before creating the tray icon anyway (an
// XEmbed-only tray does not report availability but can still host the icon).
constexpr int kTrayWaitMaxAttempts = 30;

// Em dash shown in the ID and password fields while the value is unknown.
const QChar kUnknownValue(0x2014);

//--------------------------------------------------------------------------------------------------
// Groups the digits of the ID in threes ("902 969 984") so it is easier to read and dictate.
QString formatHostId(HostId host_id)
{
    QString id = QString::number(host_id);
    for (int i = id.size() - 3; i > 0; i -= 3)
        id.insert(i, ' ');
    return id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostWindow::HostWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(std::make_unique<Ui::HostWindow>()),
      elevate_util_(ElevateUtil::create())
{
    LOG(INFO) << "Ctor";

    UserSettings user_settings;

    ui->setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

#if defined(Q_OS_MACOS)
    // The host GUI is a menu-bar (LSUIElement) agent and has no system menu bar, so render the window
    // menu inside the window itself (as on Windows/Linux) instead of the native top bar.
    menuBar()->setNativeMenuBar(false);
#endif // defined(Q_OS_MACOS)

    ui->edit_id->setText(kUnknownValue);
    ui->edit_password->setText(kUnknownValue);

    tray_menu_.addAction(ui->action_show_chat);
    tray_menu_.addAction(ui->action_security_log);
    tray_menu_.addAction(ui->action_settings);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui->action_show_hide);
    tray_menu_.addAction(ui->action_exit);

    // Qt probes the D-Bus status-notifier tray once, when the icon object is created; an icon created
    // before the shell registers its notifier host silently falls back to XEmbed, which modern shells
    // do not display. Right after login the GUI can start ahead of the shell, so wait until the tray
    // reports availability before creating the icon.
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        createTrayIcon();
    }
    else
    {
        LOG(INFO) << "System tray is not available yet; waiting";
        tray_wait_timer_ = new QTimer(this);
        connect(tray_wait_timer_, &QTimer::timeout, this, &HostWindow::onTrayAvailabilityCheck);
        tray_wait_timer_->start(std::chrono::seconds(1));
    }

    QTimer* tray_tooltip_timer = new QTimer(this);

    connect(tray_tooltip_timer, &QTimer::timeout, this, &HostWindow::updateTrayIconTooltip);

    tray_tooltip_timer->setInterval(std::chrono::seconds(30));
    tray_tooltip_timer->start();

    quint32 one_time_sessions = user_settings.oneTimeSessions();

    ui->action_desktop_manage->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_DESKTOP);
    ui->action_terminal->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_TERMINAL);
    ui->action_file_transfer->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_FILE_TRANSFER);
    ui->action_system_info->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_SYSTEM_INFO);
    ui->action_text_chat->setChecked(one_time_sessions & proto::peer::SESSION_TYPE_CHAT);

    connect(ui->menu_access, &QMenu::triggered, this, &HostWindow::onOneTimeSessionsChanged);

    createLanguageMenu(user_settings.locale());
    createThemeMenu(user_settings.theme());

    onSettingsChanged();

    connect(ui->menu_language, &QMenu::triggered, this, &HostWindow::onLanguageChanged);
    connect(ui->menu_theme, &QMenu::triggered, this, &HostWindow::onThemeChanged);
    connect(ui->action_show_chat, &QAction::triggered, this, &HostWindow::onShowChat);
    connect(ui->action_security_log, &QAction::triggered, this, &HostWindow::onSecurityLog);
    connect(ui->action_settings, &QAction::triggered, this, &HostWindow::onSettings);
    connect(ui->action_show_hide, &QAction::triggered, this, &HostWindow::onShowHide);
    connect(ui->action_exit, &QAction::triggered, this, &HostWindow::onExit);
    connect(ui->action_help, &QAction::triggered, this, &HostWindow::onHelp);
    connect(ui->action_about, &QAction::triggered, this, &HostWindow::onAbout);

    connect(ui->button_new_password, &QPushButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] New password";
        emit sig_updateCredentials(proto::user::CredentialsRequest::NEW_PASSWORD);
    });

    chat_widget_ = new ChatWidget();
    chat_widget_->setWindowFlag(Qt::WindowStaysOnTopHint);
    chat_widget_->setHistoryId(QString());
    chat_widget_->setChatEnabled(false);

    connect(chat_widget_, &ChatWidget::sig_sendMessage,
            this, [this](const proto::chat::Message& message)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_message()->CopyFrom(message);
        emit sig_chat(chat);
    });

    connect(chat_widget_, &ChatWidget::sig_sendStatus,
            this, [this](const proto::chat::Status& status)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_status()->CopyFrom(status);
        emit sig_chat(chat);
    });

    connect(GuiApplication::instance(), &GuiApplication::sig_themeChanged,
            this, &HostWindow::onAfterThemeChanged);

    onAfterThemeChanged();
}

//--------------------------------------------------------------------------------------------------
HostWindow::~HostWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void HostWindow::connectToService()
{
    if (connected_to_service_)
    {
        LOG(INFO) << "Already connected to service";
        emit sig_oneTimeSessions(calcOneTimeSessions());
        emit sig_updateCredentials(proto::user::CredentialsRequest::REFRESH);
        return;
    }

    // Drop any clipboard left over from a previous agent before wiring up a new one.
    clipboard_.reset();

    agent_ = new UserSessionAgent();
    agent_->moveToThread(GuiApplication::ioThread());

    connect(agent_, &UserSessionAgent::sig_statusChanged, this, &HostWindow::onStatusChanged,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_clientListChanged, this, &HostWindow::onClientListChanged,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_credentialsChanged, this, &HostWindow::onCredentialsChanged,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_routerStateChanged, this, &HostWindow::onRouterStateChanged,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_confirmationRequest, this, &HostWindow::onConfirmationRequest,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_recordingStateChanged, this, &HostWindow::onRecordingStateChanged,
            Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_chat, this, &HostWindow::onChat,
            Qt::QueuedConnection);

    connect(this, &HostWindow::sig_connectToService, agent_, &UserSessionAgent::onConnectToService,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_updateCredentials, agent_, &UserSessionAgent::onUpdateCredentials,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_oneTimeSessions, agent_, &UserSessionAgent::onOneTimeSessions,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_killClient, agent_, &UserSessionAgent::onStopClient,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_connectConfirmation, agent_, &UserSessionAgent::onConnectConfirmation,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_mouseLock, agent_, &UserSessionAgent::onMouseLock,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_keyboardLock, agent_, &UserSessionAgent::onKeyboardLock,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_pause, agent_, &UserSessionAgent::onPause,
            Qt::QueuedConnection);
    connect(this, &HostWindow::sig_chat, agent_, &UserSessionAgent::onChat,
            Qt::QueuedConnection);

    LOG(INFO) << "Connecting to service";
    emit sig_connectToService();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::activateHost(bool hidden)
{
    LOG(INFO) << "Activating host (hidden:" << hidden << ")";

    // A hidden activation comes from the service relaunching the GUI (e.g. after the service
    // restarted): only re-establish the service connection, without popping up the window. A
    // non-hidden activation means the user started the host application manually, so show the
    // window as well.
    if (!hidden)
    {
        show();
        activateWindow();
    }

    connectToService();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::hideToTray()
{
    LOG(INFO) << "Hide application to system tray";

    ui->action_show_hide->setText(tr("Show"));
    setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::closeEvent(QCloseEvent* event)
{
    bool hide_to_tray = !should_be_quit_;

#if defined(Q_OS_MACOS)
    // Only the window's own close button (a spontaneous close) hides to the tray. A non-spontaneous
    // close is an application-termination request - macOS "Quit & Reopen" after a permission change,
    // Cmd+Q, the Exit action - and must actually quit so macOS can relaunch the GUI.
    hide_to_tray = hide_to_tray && event->spontaneous();
#endif // defined(Q_OS_MACOS)

    if (hide_to_tray)
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
void HostWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!height_pinned_)
    {
        height_pinned_ = true;
        setFixedHeight(minimumSizeHint().height());
    }

#if defined(Q_OS_MACOS)
    // When the window is brought up (never on a silent --hidden start, which does not show it), prompt
    // for the privacy permissions the host needs if any are still missing. The dialog is modeless so it
    // never blocks the event loop - the app must stay able to quit (e.g. macOS "Quit & Reopen" restarts
    // it so a fresh process re-reads the just-granted Screen Recording permission).
    QTimer::singleShot(0, this, [this]()
    {
        if (!isVisible() || !PermissionDialog::hasMissingPermissions())
            return;

        if (permission_dialog_)
        {
            permission_dialog_->raise();
            permission_dialog_->activateWindow();
            return;
        }

        permission_dialog_ = new PermissionDialog(this);
        permission_dialog_->setAttribute(Qt::WA_DeleteOnClose);
        permission_dialog_->show();
    });
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onStatusChanged(UserSessionAgent::Status status)
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
        agent_.reset();
        realClose();
    }
    else if (status == UserSessionAgent::Status::SERVICE_NOT_AVAILABLE)
    {
        LOG(INFO) << "The connection to the service has not been established. "
                     "The application works offline.";
        connected_to_service_ = false;
        agent_.reset();
    }
    else
    {
        LOG(ERROR) << "Unandled status code: " << static_cast<int>(status);
    }

    chat_widget_->setChatEnabled(connected_to_service_);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!notifier_)
    {
        LOG(INFO) << "Create NotifierWindow";
        notifier_ = new NotifierWindow();

        connect(notifier_, &NotifierWindow::sig_killSession, this, &HostWindow::onKillSession);
        connect(notifier_, &NotifierWindow::sig_lockMouse, this, &HostWindow::sig_mouseLock);
        connect(notifier_, &NotifierWindow::sig_lockKeyboard, this, &HostWindow::sig_keyboardLock);
        connect(notifier_, &NotifierWindow::sig_pause, this, &HostWindow::sig_pause);

        notifier_->setAttribute(Qt::WA_DeleteOnClose);
        notifier_->show();
        notifier_->raise();
    }
    else
    {
        LOG(INFO) << "NotifierWindow already exists";
    }

    notifier_->onClientListChanged(clients);

    bool has_desktop = false;
    for (const auto& client : std::as_const(clients))
    {
        if (client.session_type == proto::peer::SESSION_TYPE_DESKTOP)
        {
            has_desktop = true;
            break;
        }
    }

    if (!has_desktop)
    {
        clipboard_.reset();
        return;
    }

    if (clipboard_ || !agent_)
        return;

    clipboard_ = Clipboard::create(this);
    if (!clipboard_)
        return;

    connect(clipboard_, &Clipboard::sig_clipboardEvent,
            agent_, &UserSessionAgent::onClipboardEvent, Qt::QueuedConnection);
    connect(clipboard_, &Clipboard::sig_localFileListChanged,
            agent_, &UserSessionAgent::onClipboardLocalFileListChanged, Qt::QueuedConnection);
    connect(clipboard_, &Clipboard::sig_fileDataRequest,
            agent_, &UserSessionAgent::onClipboardFileDataRequest, Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_injectClipboardEvent,
            clipboard_, &Clipboard::injectClipboardEvent, Qt::QueuedConnection);
    connect(agent_, &UserSessionAgent::sig_clipboardFileData,
            clipboard_, &Clipboard::addFileData, Qt::QueuedConnection);

    clipboard_->start();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onCredentialsChanged(const proto::user::Credentials& credentials)
{
    LOG(INFO) << "Credentials changed (host_id=" << credentials.host_id() << ")";

    ui->button_new_password->setEnabled(true);

    bool has_id = credentials.host_id() != kInvalidHostId;

    ui->edit_id->setEnabled(has_id);
    ui->edit_id->setText(has_id ? formatHostId(credentials.host_id()) : tr("Not available"));

    bool has_password = !credentials.password().empty();

    ui->button_new_password->setEnabled(has_password);
    ui->edit_password->setEnabled(has_password);
    ui->edit_password->setText(QString::fromStdString(credentials.password()));

    updateTrayIconTooltip();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onRouterStateChanged(const proto::user::RouterState& state)
{
    LOG(INFO) << "Router state changed (state=" << state.state() << ")";
    last_state_ = state.state();

    QString router;

    if (state.state() != proto::user::RouterState::DISABLED)
    {
        Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(QString::fromStdString(state.host_name()));
        address.setPort(static_cast<quint16>(state.host_port()));

        router = address.toString();
    }

    if (state.state() == proto::user::RouterState::DISABLED)
        ui->button_status->setEnabled(false);
    else
        ui->button_status->setEnabled(true);

    if (state.state() != proto::user::RouterState::CONNECTED)
    {
        ui->button_new_password->setEnabled(false);

        ui->edit_id->setText(kUnknownValue);
        ui->edit_password->setText(kUnknownValue);
    }
    else
    {
        ui->button_status->setEnabled(true);
    }

    QString status;

    switch (state.state())
    {
        case proto::user::RouterState::DISABLED:
            status = tr("Router is disabled");
            break;

        case proto::user::RouterState::CONNECTING:
            status = tr("Connecting to router %1...").arg(router);
            break;

        case proto::user::RouterState::CONNECTED:
            status = tr("Connected to router %1").arg(router);
            break;

        case proto::user::RouterState::FAILED:
            status = tr("Failed to connect to router %1").arg(router);
            break;

        default:
            NOTREACHED();
            return;
    }

    updateStatusBar();

    if (!status_dialog_)
    {
        status_dialog_ = new StatusDialog(this);

        connect(ui->button_status, &QPushButton::clicked,
                status_dialog_, &StatusDialog::show);
    }

    status_dialog_->addMessage(status);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onConfirmationRequest(const proto::user::ConfirmationRequest& request)
{
    LOG(INFO) << "Confirmation request (id=" << request.id() << ")";

    ConnectConfirmDialog dialog(request, this);
    bool accept = dialog.exec() == ConnectConfirmDialog::Accepted;

    LOG(INFO) << "[ACTION] User" << (accept ? "ACCEPT" : "REJECT") << "connection request";
    emit sig_connectConfirmation(request.id(), accept);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onRecordingStateChanged(bool started)
{
    LOG(INFO) << "Video recording state changed:" << started;
    QString message;

    if (started)
        message = tr("Screen recording has started.");
    else
        message = tr("Screen recording stopped.");

    if (tray_icon_)
        tray_icon_->showMessage(tr("Aspia Host"), message, QIcon(":/img/aspia.ico"), 1200);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onChat(const proto::chat::Chat& chat)
{
    if (chat.has_chat_message())
    {
        chat_widget_->readMessage(chat.chat_message());
        chat_widget_->show();
        chat_widget_->activateWindow();
    }
    else if (chat.has_chat_status())
    {
        chat_widget_->readStatus(chat.chat_status());
    }
    else
    {
        LOG(ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::realClose()
{
    LOG(INFO) << "realClose called";

    agent_.reset();
    should_be_quit_ = true;
    close();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<LanguageAction*>(action)->locale();

    LOG(INFO) << "[ACTION] Language changed:" << new_locale;

    Application* application = Application::instance();

    UserSettings user_settings;
    user_settings.setLocale(new_locale);
    application->setLocale(new_locale);

    ui->retranslateUi(this);

    for (QAction* theme_action : ui->menu_theme->actions())
    {
        const QString theme_id = theme_action->data().toString();
        if (!theme_id.isEmpty())
            theme_action->setText(GuiApplication::themeName(theme_id));
    }

    if (notifier_)
        notifier_->retranslateUi();

    if (status_dialog_)
        status_dialog_->retranslateUi();

    updateStatusBar();
    emit sig_updateCredentials(proto::user::CredentialsRequest::REFRESH);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onThemeChanged(QAction* action)
{
    if (!action)
        return;

    const QString theme_id = action->data().toString();
    if (theme_id.isEmpty())
        return;

    GuiApplication::instance()->setTheme(theme_id);

    UserSettings user_settings;
    user_settings.setTheme(theme_id);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onAfterThemeChanged()
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

        static const QString kLabelStyle =
            "QLabel {"
                "font: bold 11px;"
                "padding-left: 3px;"
            "}";

        QPalette window_palette = palette();
        QString edit_color = window_palette.color(QPalette::Window).name(QColor::HexRgb);

        ui->label_id->setStyleSheet(kLabelStyle);
        ui->label_password->setStyleSheet(kLabelStyle);

        set_edit_colors(ui->edit_id, edit_color);
        set_edit_colors(ui->edit_password, edit_color);

        ui->button_status->setStyleSheet("QPushButton {"
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
void HostWindow::onShowChat()
{
    chat_widget_->show();
    chat_widget_->activateWindow();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onSecurityLog()
{
    LOG(INFO) << "[ACTION] Security Log";

    if (elevate_util_ && elevate_util_->runElevated("--security-log", winId(), [this]()
    {
        ui->action_security_log->setEnabled(true);
    }))
    {
        ui->action_security_log->setEnabled(false);
        return;
    }

    SecurityLogDialog(this).exec();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onSettings()
{
    LOG(INFO) << "[ACTION] Settings";

    if (elevate_util_ && elevate_util_->runElevated("--config", winId(), [this]()
    {
        ui->action_settings->setEnabled(true);
        onSettingsChanged();
    }))
    {
        ui->action_settings->setEnabled(false);
        return;
    }

    switch (Database::instance().passwordProtectionState())
    {
        case Database::PasswordProtection::DISABLED:
        {
            ConfigDialog(this).exec();
            onSettingsChanged();
        }
        break;

        case Database::PasswordProtection::ENABLED:
        {
            CheckPasswordDialog dialog(this);
            if (dialog.exec() == CheckPasswordDialog::Accepted)
            {
                ConfigDialog(this).exec();
                onSettingsChanged();
            }
        }
        break;

        case Database::PasswordProtection::UNAVAILABLE:
        {
            MsgBox::warning(this, tr("Settings storage is unavailable."));
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onShowHide()
{
    if (isVisible())
    {
        ui->action_show_hide->setText(tr("Show"));
        setVisible(false);
    }
    else
    {
        ui->action_show_hide->setText(tr("Hide"));
        setVisible(true);
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onHelp()
{
    LOG(INFO) << "[ACTION] Help";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onAbout()
{
    LOG(INFO) << "[ACTION] About";
    AboutDialog(tr("Aspia Host"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onExit()
{
    // If the connection to the service is not established, then exit immediately.
    if (!connected_to_service_)
    {
        LOG(INFO) << "No agent proxy";
        realClose();
        return;
    }

    LOG(INFO) << "[ACTION] Exit";

    if (MsgBox::question(this,
            tr("If you exit from Aspia, it will not be possible to connect to this computer until "
               "you turn on the computer or Aspia again manually. Do you really want to exit the "
               "application?")) == MsgBox::Yes)
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
            connect(notifier_, &NotifierWindow::sig_finished, this, &HostWindow::realClose);
            notifier_->onStop();
        }
    }
    else
    {
        LOG(INFO) << "[ACTION] User rejected exit";
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onSettingsChanged()
{
    LOG(INFO) << "Settings changed";
    SystemSettings settings;
    ui->action_exit->setEnabled(!settings.isApplicationShutdownDisabled());
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onKillSession(quint32 session_id)
{
    LOG(INFO) << "Killing session with ID:" << session_id;
    emit sig_killClient(session_id);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onOneTimeSessionsChanged()
{
    LOG(INFO) << "[ACTION] One-time sessions changed";

    quint32 sessions = calcOneTimeSessions();

    UserSettings user_settings;
    user_settings.setOneTimeSessions(sessions);

    emit sig_oneTimeSessions(sessions);
}

//--------------------------------------------------------------------------------------------------
void HostWindow::onTrayAvailabilityCheck()
{
    ++tray_wait_attempts_;
    if (!QSystemTrayIcon::isSystemTrayAvailable() && tray_wait_attempts_ < kTrayWaitMaxAttempts)
        return;

    if (tray_wait_attempts_ >= kTrayWaitMaxAttempts)
        LOG(INFO) << "System tray still not available; creating the icon anyway";
    else
        LOG(INFO) << "System tray became available on attempt" << tray_wait_attempts_;

    tray_wait_timer_->stop();
    tray_wait_timer_->deleteLater();
    tray_wait_timer_ = nullptr;

    createTrayIcon();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::createLanguageMenu(const QString& current_locale)
{
    Application::LocaleList locale_list = GuiApplication::instance()->localeList();
    if (locale_list.isEmpty())
        return;

    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : std::as_const(locale_list))
    {
        LanguageAction* action_language =
            new LanguageAction(locale.first, locale.second, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale.first)
            action_language->setChecked(true);

        ui->menu_language->addAction(action_language);
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::createThemeMenu(const QString& current_theme)
{
    GuiApplication* app = GuiApplication::instance();
    const QStringList available_themes = app->availableThemes();
    QActionGroup* theme_group = new QActionGroup(this);

    theme_group->setExclusive(true);

    for (const QString& theme_id : available_themes)
    {
        QAction* action = new QAction(GuiApplication::themeName(theme_id), this);
        action->setCheckable(true);
        action->setData(theme_id);
        action->setActionGroup(theme_group);
        action->setChecked(theme_id == current_theme);
        ui->menu_theme->addAction(action);
    }
}

//--------------------------------------------------------------------------------------------------
void HostWindow::createTrayIcon()
{
    tray_icon_.reset(new QSystemTrayIcon(this));
    tray_icon_->setIcon(QIcon(":/img/aspia.ico"));

#if !defined(Q_OS_MACOS)
    tray_icon_->setContextMenu(&tray_menu_);
#endif // !defined(Q_OS_MACOS)

    connect(tray_icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Context)
        {
#if defined(Q_OS_MACOS)
            // A status item with an attached menu pops it on every click (left too). Leave it detached
            // and show it here on right-click only, so left-click just toggles the window (as on Windows).
            tray_menu_.popup(QCursor::pos());
#endif // defined(Q_OS_MACOS)
            return;
        }

        LOG(INFO) << "[ACTION] Tray icon activated";
        onShowHide();
    });

    tray_icon_->show();
    updateTrayIconTooltip();
}

//--------------------------------------------------------------------------------------------------
void HostWindow::updateStatusBar()
{
    LOG(INFO) << "Update status bar";

    QString message;
    QString icon;

    switch (last_state_)
    {
        case proto::user::RouterState::DISABLED:
            message = tr("Router is disabled");
            icon = ":/img/close.svg";
            break;

        case proto::user::RouterState::CONNECTING:
            message = tr("Connecting to router...");
            icon = ":/img/replay.svg";
            break;

        case proto::user::RouterState::CONNECTED:
            message = tr("Connected to router");
            icon = ":/img/done.svg";
            break;

        case proto::user::RouterState::FAILED:
            message = tr("Connection error");
            icon = ":/img/close.svg";
            break;

        default:
            NOTREACHED();
            return;
    }

    ui->button_status->setText(message);
    ui->button_status->setIcon(QIcon(icon));

    QString statusbar_text_color = palette().color(QPalette::WindowText).name(QColor::HexRgb);

    ui->button_status->setStyleSheet(QString("QPushButton {"
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
void HostWindow::updateTrayIconTooltip()
{
    if (!tray_icon_)
        return;

    QString ipv4;
    QString ipv6;

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
            const QNetworkAddressEntry& entry = addresses[j];
            QHostAddress address = entry.ip();
            if (address.isLoopback() || address.isLinkLocal())
                continue;

            if (address.protocol() == QAbstractSocket::IPv4Protocol)
                ipv4 += address.toString() + '\n';
            else if (address.protocol() == QAbstractSocket::IPv6Protocol && !entry.isTemporary())
                ipv6 += address.toString() + '\n';
        }
    }

    QString ip = ipv4 + ipv6;

    if (!ip.isEmpty())
        ip.prepend(tr("IP addresses:") + '\n');

    QString tooltip;
    tooltip += tr("Aspia Host") + "\n\n";
    tooltip += tr("ID: %1").arg(ui->edit_id->text()) + '\n';
    tooltip += ip;

    tray_icon_->setToolTip(tooltip);
}

//--------------------------------------------------------------------------------------------------
quint32 HostWindow::calcOneTimeSessions()
{
    quint32 sessions = 0;

    if (ui->action_desktop_manage->isChecked())
        sessions |= proto::peer::SESSION_TYPE_DESKTOP;

    if (ui->action_terminal->isChecked())
        sessions |= proto::peer::SESSION_TYPE_TERMINAL;

    if (ui->action_file_transfer->isChecked())
        sessions |= proto::peer::SESSION_TYPE_FILE_TRANSFER;

    if (ui->action_system_info->isChecked())
        sessions |= proto::peer::SESSION_TYPE_SYSTEM_INFO;

    if (ui->action_text_chat->isChecked())
        sessions |= proto::peer::SESSION_TYPE_CHAT;

    return sessions;
}
