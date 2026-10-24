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

#ifndef CLIENT_ANDROID_HOST_KEY_DIALOG_H
#define CLIENT_ANDROID_HOST_KEY_DIALOG_H

#include <QByteArray>

#include "common/android/dialog.h"

// Asks the user to trust the identity key of a host (operating system / domain accounts). Shown
// when the key is not pinned yet or has changed; no credentials have been sent at that point.
class HostKeyDialog final : public Dialog
{
    Q_OBJECT

public:
    HostKeyDialog(const QString& address, const QByteArray& public_key, bool key_changed,
                  QWidget* parent = nullptr);
    ~HostKeyDialog() final;

    // Shows the dialog and, if the user trusts the key, pins it in the database. Returns true if
    // the key is pinned and the connection may be started again.
    static bool confirm(QWidget* parent, const QString& address, const QByteArray& public_key,
                        bool key_changed);

private:
    Q_DISABLE_COPY_MOVE(HostKeyDialog)
};

#endif // CLIENT_ANDROID_HOST_KEY_DIALOG_H
