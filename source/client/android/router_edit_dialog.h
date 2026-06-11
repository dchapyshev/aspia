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

#ifndef CLIENT_ANDROID_ROUTER_EDIT_DIALOG_H
#define CLIENT_ANDROID_ROUTER_EDIT_DIALOG_H

#include <QByteArray>

#include "common/android/dialog.h"

class Label;
class LineEdit;

// Dialog for adding a new router or editing an existing one. Pass a router id to edit, or -1 to add.
class RouterEditDialog final : public Dialog
{
    Q_OBJECT

public:
    explicit RouterEditDialog(qint64 router_id, QWidget* parent = nullptr);
    ~RouterEditDialog() final;

private slots:
    void onSaveClicked();

private:
    void showError(const QString& message);

    LineEdit* name_ = nullptr;
    LineEdit* address_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    Label* error_ = nullptr;
    qint64 router_id_ = -1;
    QByteArray encrypted_device_token_;

    Q_DISABLE_COPY_MOVE(RouterEditDialog)
};

#endif // CLIENT_ANDROID_ROUTER_EDIT_DIALOG_H
