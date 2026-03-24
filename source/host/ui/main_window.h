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

#ifndef HOST_UI_MAIN_WINDOW_H
#define HOST_UI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>

#include "host/user_session_agent.h"
#include "ui_main_window.h"

namespace common {
class StatusDialog;
class ChatWidget;
} // namespace common

namespace host {

class NotifierWindow;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() final;

public slots:
    void connectToService();
    void activateHost();
    void hideToTray();

signals:
    void sig_connectToService();
    void sig_disconnectFromService();
    void sig_updateCredentials(proto::user::CredentialsRequest_Type type);
    void sig_oneTimeSessions(quint32 sessions);
    void sig_killClient(quint32 id);
    void sig_connectConfirmation(quint32 id, bool accept);
    void sig_mouseLock(bool enable);
    void sig_keyboardLock(bool enable);
    void sig_pause(bool enable);
    void sig_chat(const proto::chat::Chat& chat);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onStatusChanged(host::UserSessionAgent::Status status);
    void onClientListChanged(const host::UserSessionAgent::ClientList& clients);
    void onCredentialsChanged(const proto::user::Credentials& credentials);
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onConfirmationRequest(const proto::user::ConfirmationRequest& request);
    void onRecordingStateChanged(const QString& computer, const QString& user, bool started);
    void onChat(const proto::chat::Chat& chat);

    void realClose();
    void onLanguageChanged(QAction* action);
    void onThemeChanged(QAction* action);
    void onAfterThemeChanged();
    void onShowChat();
    void onSettings();
    void onShowHide();
    void onHelp();
    void onAbout();
    void onExit();
    void onSettingsChanged();
    void onKillSession(quint32 session_id);
    void onOneTimeSessionsChanged();

private:
    void createLanguageMenu(const QString& current_locale);
    void createThemeMenu(const QString& current_theme);
    void updateStatusBar();
    void updateTrayIconTooltip();
    quint32 calcOneTimeSessions();

    Ui::MainWindow ui;

    bool should_be_quit_ = false;
    bool connected_to_service_ = false;

    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;
    QPointer<NotifierWindow> notifier_;
    common::ChatWidget* chat_widget_ = nullptr;

    common::StatusDialog* status_dialog_ = nullptr;
    proto::user::RouterState::State last_state_ = proto::user::RouterState::DISABLED;

    Q_DISABLE_COPY_MOVE(MainWindow)
};

} // namespace host

#endif // HOST_UI_MAIN_WINDOW_H
