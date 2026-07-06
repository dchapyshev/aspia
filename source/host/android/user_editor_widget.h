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

#ifndef HOST_ANDROID_USER_EDITOR_WIDGET_H
#define HOST_ANDROID_USER_EDITOR_WIDGET_H

#include "base/peer/user.h"
#include "common/android/scroll_area.h"

class Button;
class Label;
class LineEdit;
class Switch;

class UserEditorWidget final : public ScrollArea
{
    Q_OBJECT

public:
    explicit UserEditorWidget(QWidget* parent = nullptr);
    ~UserEditorWidget() final;

    // Loads the user with |entry_id| for editing, or an empty form when |entry_id| is 0 (new user).
    void load(qint64 entry_id);

    // True while editing an existing user (drives the screen title).
    bool isEditing() const { return entry_id_ != 0; }

    // Validates the form and writes the user to the database. On success sig_saved() is emitted; on a
    // validation error a message is shown and nothing is written.
    void save();

signals:
    // The user was created, modified or deleted; the caller should return to the list.
    void sig_saved();

private:
    void deleteUser();

    Label* sessions_header_ = nullptr;
    Label* password_hint_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    LineEdit* password_repeat_ = nullptr;
    Switch* enabled_ = nullptr;
    Switch* desktop_ = nullptr;
    Switch* file_transfer_ = nullptr;
    Button* delete_button_ = nullptr;

    qint64 entry_id_ = 0;
    User user_;

    Q_DISABLE_COPY_MOVE(UserEditorWidget)
};

#endif // HOST_ANDROID_USER_EDITOR_WIDGET_H
