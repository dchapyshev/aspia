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

#ifndef CLIENT_ANDROID_MASTER_PASSWORD_DIALOG_H
#define CLIENT_ANDROID_MASTER_PASSWORD_DIALOG_H

#include "common/android/dialog.h"

class Label;
class LineEdit;

// Dialog gating the application behind the master password. In CREATE mode it sets a new password
// (first launch), in UNLOCK mode it verifies the existing one. It accepts only once the data cryptor
// is unlocked.
class MasterPasswordDialog final : public Dialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        CREATE,
        UNLOCK,
        CHANGE
    };

    explicit MasterPasswordDialog(Mode mode, QWidget* parent = nullptr);
    ~MasterPasswordDialog() final;

private slots:
    void onAccept();
    void tryBiometricUnlock();

private:
    void showError(const QString& message);

    // Discards a broken or invalidated biometric enrollment and reports the reason to the user.
    void dropBiometric(const QString& message);

    LineEdit* current_ = nullptr;
    LineEdit* password_ = nullptr;
    LineEdit* confirm_ = nullptr;
    Label* error_ = nullptr;
    Mode mode_ = Mode::UNLOCK;

    Q_DISABLE_COPY_MOVE(MasterPasswordDialog)
};

#endif // CLIENT_ANDROID_MASTER_PASSWORD_DIALOG_H
