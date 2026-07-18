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

#ifndef HOST_ANDROID_MAIN_WINDOW_H
#define HOST_ANDROID_MAIN_WINDOW_H

#include <QList>
#include <QPointer>
#include <QWidget>

#include "host/android/server_worker.h"

class AppBar;
class BottomNavigationBar;
class ConnectionWidget;
class QScrollArea;
class QStackedWidget;

// Top-level application window for the Android host: a top app bar, a content area switched by the
// bottom navigation bar between the Connection and Settings sections.
class AndroidMainWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit AndroidMainWindow(QWidget* parent = nullptr);
    ~AndroidMainWindow() final;

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onSectionChanged(int index);
    void onSettingsTitleChanged(const QString& title, bool back_visible);
    void onSettingsActionsChanged();
    void onBackClicked();

    // Lifts the content above the on-screen keyboard by the amount the keyboard overlaps the window.
    // Android's adjustResize is unreliable here (a key press on the on-screen keyboard can restore the
    // full window height with the keyboard still up), so the keyboard rectangle reported by Qt is the
    // source of truth. The bottom navigation is hidden while the keyboard is up.
    void onUpdateKeyboardInset();

    // Forwarded from the host server (queued from the I/O thread) to the connection screen.
    void onCredentialsChanged(const QString& host_id, const QString& password);
    void onRouterStateChanged(int state, const QString& router);
    void onConnectedClientsChanged(const QList<ServerWorker::ClientInfo>& clients);

private:
    void scrollFocusIntoView();
    void checkPermissions();
    void checkAccessibilityService();
    void checkOverlayPermission();
    void checkStoragePermission();
    QScrollArea* focusedScrollArea() const;
    QString sectionTitle(int index) const;

    AppBar* app_bar_ = nullptr;
    QStackedWidget* content_ = nullptr;
    BottomNavigationBar* navigation_ = nullptr;
    ConnectionWidget* connection_ = nullptr;

    QPointer<ServerWorker> server_;

    // Cached bottom inset applied for the on-screen keyboard; -1 forces the first update to apply.
    int keyboard_inset_ = -1;

    // Set once the settings tab has been unlocked with the protection password (if any); the prompt is
    // then not shown again for the rest of the session.
    bool settings_unlocked_ = false;

    Q_DISABLE_COPY_MOVE(AndroidMainWindow)
};

#endif // HOST_ANDROID_MAIN_WINDOW_H
