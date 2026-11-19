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

#ifndef CLIENT_SYNC_SYNC_CRYPTO_H
#define CLIENT_SYNC_SYNC_CRYPTO_H

#include <QByteArray>

#include <optional>

#include "proto/router_sync.h"

class SecureByteArray;
class SecureString;

class SyncCrypto
{
public:
    enum class Result { SUCCESS, WRONG_PASSWORD, INVALID_FORMAT };

    static SecureByteArray generateStorageKey();

    static bool wrapStorageKey(
        const SecureByteArray& key, const SecureString& password, proto::router::SyncStorageKey* wrapped);
    static Result unwrapStorageKey(
        const proto::router::SyncStorageKey& wrapped, const SecureString& password, SecureByteArray* key);

    // A record opens only under the type and the id it was sealed for.
    static std::optional<QByteArray> encryptRecord(
        const SecureByteArray& key, proto::router::SyncDataType type, std::string_view id, QByteArrayView data);
    static std::optional<QByteArray> decryptRecord(
        const SecureByteArray& key, proto::router::SyncDataType type, std::string_view id, QByteArrayView blob);

private:
    Q_DISABLE_COPY_MOVE(SyncCrypto)
};

#endif // CLIENT_SYNC_SYNC_CRYPTO_H
