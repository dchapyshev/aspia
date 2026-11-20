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

#ifndef CLIENT_STORAGE_UNLOCK_STORE_H
#define CLIENT_STORAGE_UNLOCK_STORE_H

#include "client/storage/database_store.h"

class UnlockStore final : public DatabaseStore
{
public:
    explicit UnlockStore(Database& db = Database::instance());

    // Master password.
    bool isMasterPasswordSet() const;
    QByteArray masterPasswordSalt() const;
    QByteArray masterPasswordVerifier() const;
    quint32 masterPasswordVersion() const;

    // Automatic unlock on startup.
    QByteArray autoUnlockBlob() const;
    bool setAutoUnlockBlob(const QByteArray& blob);

    // Biometric unlock.
    bool isBiometricUnlockEnabled() const;
    QByteArray biometricBlob() const;
    bool setBiometricBlob(const QByteArray& blob);
    bool clearBiometricUnlock();

private:
    friend class Database::Reencryption;

    bool setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version);
};

#endif // CLIENT_STORAGE_UNLOCK_STORE_H
