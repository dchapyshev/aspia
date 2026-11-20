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

#include "client/storage/unlock_store.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
UnlockStore::UnlockStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::isMasterPasswordSet() const
{
    SqlTransaction transaction(sql());
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql().lastError();
        return false;
    }

    return !readSetting(kSettingSalt).isEmpty() && !readSetting(kSettingVerifier).isEmpty();
}

//--------------------------------------------------------------------------------------------------
QByteArray UnlockStore::masterPasswordSalt() const
{
    return QByteArray::fromBase64(readSetting(kSettingSalt).toLatin1());
}

//--------------------------------------------------------------------------------------------------
QByteArray UnlockStore::masterPasswordVerifier() const
{
    return QByteArray::fromBase64(readSetting(kSettingVerifier).toLatin1());
}

//--------------------------------------------------------------------------------------------------
quint32 UnlockStore::masterPasswordVersion() const
{
    return readSetting(kSettingVersion).toUInt();
}

//--------------------------------------------------------------------------------------------------
QByteArray UnlockStore::autoUnlockBlob() const
{
    return QByteArray::fromBase64(readSetting(kSettingAutoUnlock).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::setAutoUnlockBlob(const QByteArray& blob)
{
    return writeSetting(kSettingAutoUnlock, QString::fromLatin1(blob.toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::isBiometricUnlockEnabled() const
{
    // The presence of a wrapped key is the single source of truth for the feature being enabled.
    return !biometricBlob().isEmpty();
}

//--------------------------------------------------------------------------------------------------
QByteArray UnlockStore::biometricBlob() const
{
    return QByteArray::fromBase64(readSetting(kSettingBiometricBlob).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::setBiometricBlob(const QByteArray& blob)
{
    return writeSetting(kSettingBiometricBlob, QString::fromLatin1(blob.toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::clearBiometricUnlock()
{
    // Overwrite the wrapped key so the stored blob does not survive disabling the feature
    // (the database runs with secure_delete enabled).
    return writeSetting(kSettingBiometricBlob, QString());
}

//--------------------------------------------------------------------------------------------------
bool UnlockStore::setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version)
{
    return writeSetting(kSettingSalt, QString::fromLatin1(salt.toBase64())) &&
           writeSetting(kSettingVerifier, QString::fromLatin1(verifier.toBase64())) &&
           writeSetting(kSettingVersion, QString::number(version));
}
