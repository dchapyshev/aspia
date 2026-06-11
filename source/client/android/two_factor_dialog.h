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

#ifndef CLIENT_ANDROID_TWO_FACTOR_DIALOG_H
#define CLIENT_ANDROID_TWO_FACTOR_DIALOG_H

#include "common/android/dialog.h"

class LineEdit;

// Collects the TOTP code required by a router. When |otpauth_uri| is non-empty the dialog also
// shows the setup key for first-time enrollment in an authenticator app.
class TwoFactorDialog final : public Dialog
{
    Q_OBJECT

public:
    explicit TwoFactorDialog(const QString& otpauth_uri, QWidget* parent = nullptr);
    ~TwoFactorDialog() final;

    QString code() const;

private:
    LineEdit* code_ = nullptr;

    Q_DISABLE_COPY_MOVE(TwoFactorDialog)
};

#endif // CLIENT_ANDROID_TWO_FACTOR_DIALOG_H
