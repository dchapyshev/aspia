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

#ifndef CLIENT_ANDROID_PASSWORD_DIALOG_H
#define CLIENT_ANDROID_PASSWORD_DIALOG_H

#include "base/crypto/secure_string.h"
#include "common/android/dialog.h"

class Label;
class LineEdit;

// Prompts for a password. In SET mode the password is entered twice and must match; in ENTER mode
// a single field is shown. The entered value is read with password() after the dialog is accepted.
class PasswordDialog final : public Dialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        ENTER,
        SET
    };

    explicit PasswordDialog(Mode mode, QWidget* parent = nullptr);
    ~PasswordDialog() final;

    SecureString password() const;

private slots:
    void onAcceptClicked();

private:
    void showError(const QString& message);

    Mode mode_;
    LineEdit* password_ = nullptr;
    LineEdit* confirm_ = nullptr;
    Label* error_ = nullptr;

    Q_DISABLE_COPY_MOVE(PasswordDialog)
};

#endif // CLIENT_ANDROID_PASSWORD_DIALOG_H
