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

#include <QWidget>

class AppBar;
class BottomNavigationBar;
class DesktopWindow;
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

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

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
    void onConnectHost(qint64 entry_id);
    void onConnectRouterHost(const HostConfig& host);
    void onDesktopClosed();

private:
    // Gates the window behind the master password: prompts to create or unlock it, and reloads the
    // content that depends on the unlocked data cryptor once it is open.
    void runMasterPasswordGate();
    void onUnlocked();
    void retranslate();

    // Replaces the address book with a full-screen desktop view for the given host. Only a single
    // desktop connection is supported at a time.
    void openDesktop(const HostConfig& host);

    QStackedWidget* root_stack_ = nullptr;
    QWidget* shell_ = nullptr;
    AppBar* app_bar_ = nullptr;
    QStackedWidget* content_ = nullptr;
    BottomNavigationBar* navigation_ = nullptr;
    DesktopWindow* desktop_ = nullptr;

    Q_DISABLE_COPY_MOVE(AndroidMainWindow)
};

#endif // CLIENT_ANDROID_MAIN_WINDOW_H
