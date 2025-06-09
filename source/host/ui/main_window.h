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

#ifndef HOST_UI_MAIN_WINDOW_H
#define HOST_UI_MAIN_WINDOW_H

#include "host/user_session_agent.h"
#include "ui_main_window.h"

#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>

namespace common {
class StatusDialog;
class TextChatWidget;
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
    void sig_updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void sig_oneTimeSessions(quint32 sessions);
    void sig_killClient(quint32 id);
    void sig_connectConfirmation(quint32 id, bool accept);
    void sig_mouseLock(bool enable);
    void sig_keyboardLock(bool enable);
    void sig_pause(bool enable);
    void sig_textChat(const proto::text_chat::TextChat& text_chat);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onStatusChanged(host::UserSessionAgent::Status status);
    void onClientListChanged(const host::UserSessionAgent::ClientList& clients);
    void onCredentialsChanged(const proto::internal::Credentials& credentials);
    void onRouterStateChanged(const proto::internal::RouterState& state);
    void onConnectConfirmationRequest(
        const proto::internal::ConnectConfirmationRequest& request);
    void onVideoRecordingStateChanged(
        const QString& computer_name, const QString& user_name, bool started);
    void onTextChat(const proto::text_chat::TextChat& text_chat);

    void realClose();
    void onLanguageChanged(QAction* action);
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
    void updateStatusBar();
    void updateTrayIconTooltip();
    quint32 calcOneTimeSessions();

    Ui::MainWindow ui;

    bool should_be_quit_ = false;
    bool connected_to_service_ = false;

    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;
    QPointer<NotifierWindow> notifier_;
    QPointer<common::TextChatWidget> text_chat_widget_;

    common::StatusDialog* status_dialog_ = nullptr;
    proto::internal::RouterState::State last_state_ = proto::internal::RouterState::DISABLED;

    DISALLOW_COPY_AND_ASSIGN(MainWindow);
};

} // namespace host

#endif // HOST_UI_MAIN_WINDOW_H
