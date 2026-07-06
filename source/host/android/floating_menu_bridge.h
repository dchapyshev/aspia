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

#ifndef HOST_ANDROID_FLOATING_MENU_BRIDGE_H
#define HOST_ANDROID_FLOATING_MENU_BRIDGE_H

#include <QObject>
#include <QString>

// Bridges the session to the floating action menu (FloatingMenu.java). Android blocks clipboard access
// for apps that are not in focus, so the menu reads/writes the clipboard only when the user opens it (the
// menu window is focusable). This object shows/hides the floating button and carries text both ways: the
// remote clipboard is queued for the next sync via setIncomingText(), and text read on a sync arrives back
// through sig_clipboardText().
class FloatingMenuBridge final : public QObject
{
    Q_OBJECT

public:
    explicit FloatingMenuBridge(QObject* parent = nullptr);
    ~FloatingMenuBridge() final;

    void showButton();
    void hideButton();
    void setIncomingText(const QString& text);

    // Invoked from the JNI callback (Android main thread); re-posts to this object's thread.
    void onClipboardText(const QString& text);

signals:
    void sig_clipboardText(const QString& text);

private:
    Q_DISABLE_COPY_MOVE(FloatingMenuBridge)
};

#endif // HOST_ANDROID_FLOATING_MENU_BRIDGE_H
