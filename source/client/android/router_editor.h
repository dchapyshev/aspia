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

#ifndef CLIENT_ANDROID_ROUTER_EDITOR_H
#define CLIENT_ANDROID_ROUTER_EDITOR_H

#include <QByteArray>
#include <QWidget>

class Button;
class Label;
class LineEdit;

// Full-screen editor for a router. On save the router is written to the local database; the delete
// action is shown only when editing an existing router.
class RouterEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit RouterEditor(QWidget* parent = nullptr);
    ~RouterEditor() final;

    // Resets the form for adding a new router.
    void prepareForAdd();

    // Loads the router |router_id| into the form for editing.
    void prepareForEdit(qint64 router_id);

signals:
    // Emitted after the router has been saved to or removed from the database.
    void sig_accepted();

private slots:
    void onSaveClicked();
    void onDeleteClicked();

private:
    void showError(const QString& message);

    LineEdit* name_ = nullptr;
    LineEdit* address_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    Label* error_ = nullptr;
    Button* delete_button_ = nullptr;
    qint64 router_id_ = -1;
    QByteArray encrypted_device_token_;

    Q_DISABLE_COPY_MOVE(RouterEditor)
};

#endif // CLIENT_ANDROID_ROUTER_EDITOR_H
