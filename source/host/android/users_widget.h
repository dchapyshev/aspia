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

#ifndef HOST_ANDROID_USERS_WIDGET_H
#define HOST_ANDROID_USERS_WIDGET_H

#include "common/android/scroll_area.h"

// User management screen for the Android host: lists the configured users from the host database.
// Reached as a sub-page of the settings section; the add and back actions live in the app bar.
class UsersWidget final : public ScrollArea
{
    Q_OBJECT

public:
    explicit UsersWidget(QWidget* parent = nullptr);
    ~UsersWidget() final;

    // Rebuilds the list from the database. Called when the screen is opened.
    void reload();

    // Rebuilds the content with the current language.
    void retranslate();

signals:
    // A user row was tapped; |entry_id| identifies the user to edit.
    void sig_editUser(qint64 entry_id);

protected:
    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private:
    void buildContent();

    Q_DISABLE_COPY_MOVE(UsersWidget)
};

#endif // HOST_ANDROID_USERS_WIDGET_H
