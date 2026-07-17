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

#ifndef CLIENT_ANDROID_AUTHORIZATION_DIALOG_H
#define CLIENT_ANDROID_AUTHORIZATION_DIALOG_H

#include "base/crypto/secure_string.h"
#include "common/android/dialog.h"

class Label;
class LineEdit;
class Switch;

class AuthorizationDialog final : public Dialog
{
    Q_OBJECT

public:
    // |one_time_password_available| enables the one-time password switch (router hosts only).
    explicit AuthorizationDialog(bool one_time_password_available, QWidget* parent = nullptr);
    ~AuthorizationDialog() final;

    void setUserName(const QString& username);

    // Empty when a one-time password is used; the caller then connects by host id.
    QString userName() const;
    SecureString password() const;

private slots:
    void onAcceptClicked();
    void onOneTimePasswordToggled(bool checked);

private:
    void showError(const QString& message);

    Switch* one_time_password_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    Label* error_ = nullptr;

    Q_DISABLE_COPY_MOVE(AuthorizationDialog)
};

#endif // CLIENT_ANDROID_AUTHORIZATION_DIALOG_H
