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

#ifndef HOST_UI_HOST_WINDOW_H
#define HOST_UI_HOST_WINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>

#include <memory>

#include "base/scoped_qpointer.h"
#include "host/workers/user_ipc_worker.h"

namespace Ui {
class HostWindow;
} // namespace Ui

class ChatWidget;
class Clipboard;
class ElevateUtil;
class NotifierWindow;
class PermissionDialog;
class QTimer;
class StatusDialog;

class HostWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit HostWindow(QWidget* parent = nullptr);
    ~HostWindow() final;

public slots:
    void connectToService();
    void activateHost(bool hidden);
    void hideToTray();

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;
    void showEvent(QShowEvent* event) final;

private slots:
    void onStatusChanged(UserIpcWorker::Status status);
    void onClientListChanged(const UserIpcWorker::ClientList& clients);
    void onCredentialsChanged(const proto::user::Credentials& credentials);
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onConfirmationRequest(const proto::user::ConfirmationRequest& request);
    void onRecordingStateChanged(bool started);
    void onChat(const proto::chat::Chat& chat);

    void realClose();
    void onLanguageChanged(QAction* action);
    void onThemeChanged(QAction* action);
    void onAfterThemeChanged();
    void onShowChat();
    void onSecurityLog();
    void onSettings();
    void onShowHide();
    void onHelp();
    void onAbout();
    void onExit();
    void onSettingsChanged();
    void onKillSession(quint32 session_id);
    void onOneTimeSessionsChanged();
    void onTrayAvailabilityCheck();

private:
    void createLanguageMenu(const QString& current_locale);
    void createThemeMenu(const QString& current_theme);
    void createTrayIcon();
    void updateStatusBar();
    void updateTrayIconTooltip();
    quint32 calcOneTimeSessions();

    std::unique_ptr<Ui::HostWindow> ui;
    std::unique_ptr<ElevateUtil> elevate_util_;
    bool should_be_quit_ = false;
    bool connected_to_service_ = false;
    bool height_pinned_ = false;

    // Created only when the shell's tray is available (see createTrayIcon()).
    ScopedQPointer<QSystemTrayIcon> tray_icon_;
    ScopedQPointer<QTimer> tray_wait_timer_;
    int tray_wait_attempts_ = 0;

    QMenu tray_menu_;
    QPointer<NotifierWindow> notifier_;
    ScopedQPointer<ChatWidget> chat_widget_;

#if defined(Q_OS_MACOS)
    // The macOS privacy-permission dialog. Shown modeless so it never blocks the event loop - the app
    // must stay able to quit, e.g. for macOS "Quit & Reopen" after a permission change.
    QPointer<PermissionDialog> permission_dialog_;
#endif // defined(Q_OS_MACOS)

    StatusDialog* status_dialog_ = nullptr;

    QPointer<UserIpcWorker> ipc_worker_;
    ScopedQPointer<Clipboard> clipboard_;
    proto::user::RouterState::State last_state_ = proto::user::RouterState::DISABLED;

    Q_DISABLE_COPY_MOVE(HostWindow)
};

#endif // HOST_UI_HOST_WINDOW_H
