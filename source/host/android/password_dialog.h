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

#ifndef HOST_ANDROID_PASSWORD_DIALOG_H
#define HOST_ANDROID_PASSWORD_DIALOG_H

#include <QCoreApplication>

#include <optional>

#include "base/crypto/secure_string.h"

class QWidget;

class PasswordDialog
{
    Q_DECLARE_TR_FUNCTIONS(PasswordDialog)

public:
    // Prompts for the protection password and verifies it against the database. Returns true if the
    // entered password is correct.
    static bool verify(QWidget* parent);

    // Prompts for a new password entered twice; returns it, or nullopt if cancelled.
    static std::optional<SecureString> create(QWidget* parent);

private:
    Q_DISABLE_COPY_MOVE(PasswordDialog)
};

#endif // HOST_ANDROID_PASSWORD_DIALOG_H
