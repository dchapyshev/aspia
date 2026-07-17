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

#ifndef CLIENT_ANDROID_MAIN_WINDOW_H
#define CLIENT_ANDROID_MAIN_WINDOW_H

#include <QDateTime>
#include <QWidget>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class AppBar;
class BottomNavigationBar;
class ChatWindow;
class DesktopWindow;
class FileTransferWindow;
class HostConfig;
class LocalWidget;
class RemoteWidget;
class QStackedWidget;

// Top-level application window for the Android client: a top app bar, a content area switched by
// the bottom navigation bar between the address book, routers and settings sections.
class AndroidMainWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit AndroidMainWindow(QWidget* parent = nullptr);
    ~AndroidMainWindow() final;

private slots:
    void onSectionChanged(int index);
    void onRouterActionsChanged();
    void onLocalActionsChanged();
    void onLocalTitleChanged(const QString& title, bool back_visible);
    void onRoutersTitleChanged(const QString& title, bool back_visible);
    void onRemoteTitleChanged(const QString& title, bool back_visible);
    void onSettingsTitleChanged(const QString& title, bool back_visible);
    void onSettingsActionsChanged();
    void onSearchModeChanged(bool active);
    void onSearchTextChanged(const QString& text);
    void onBackClicked();
    void onConnectHost(qint64 entry_id, proto::peer::SessionType session_type);
    void onConnectRouterHost(const HostConfig& host, proto::peer::SessionType session_type);
    void onDesktopClosed();
    void onFileTransferClosed();
    void onChatClosed();

    // Tracks foreground/background transitions to re-lock the app after it has been in the background
    // longer than the timeout.
    void onApplicationStateChanged(Qt::ApplicationState state);

private:
    // Gates the window behind the master password: prompts to create or unlock it, and reloads the
    // content that depends on the unlocked data cryptor once it is open.
    void runMasterPasswordGate();
    void onUnlocked();

    // Prompts to unlock (password or fingerprint) again after the background timeout; quits if the
    // user cancels.
    void relock();

    // Routes a connection request to the matching session window. Only a single session is supported
    // at a time.
    void openSession(HostConfig host, proto::peer::SessionType session_type);

    // Replaces the address book with a full-screen desktop view for the given host.
    void openDesktop(const HostConfig& host);

    // Opens the file transfer screen for the given host as a regular page (not full-screen).
    void openFileTransfer(const HostConfig& host);

    // Opens the chat screen for the given host as a regular page (not full-screen).
    void openChat(const HostConfig& host);

    QStackedWidget* root_stack_ = nullptr;
    QWidget* shell_ = nullptr;
    AppBar* app_bar_ = nullptr;
    QStackedWidget* content_ = nullptr;
    BottomNavigationBar* navigation_ = nullptr;
    DesktopWindow* desktop_ = nullptr;
    FileTransferWindow* file_transfer_ = nullptr;
    ChatWindow* chat_ = nullptr;

    // When the app went to the background (invalid while in the foreground), and a guard against
    // re-entering the lock prompt while it is already shown.
    QDateTime background_since_;
    bool relocking_ = false;

    Q_DISABLE_COPY_MOVE(AndroidMainWindow)
};

#endif // CLIENT_ANDROID_MAIN_WINDOW_H
