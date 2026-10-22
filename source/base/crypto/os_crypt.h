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

#ifndef BASE_CRYPTO_OS_CRYPT_H
#define BASE_CRYPTO_OS_CRYPT_H

#include <QByteArray>
#include <QString>

class OSCrypt
{
public:
    // Encrypt an array of bytes.
    static bool encryptBytes(const QByteArray& plaintext, QByteArray* ciphertext);

    // Decrypt an array of bytes. |needs_reencrypt| (optional) is set when the bytes were sealed
    // with an outdated key (Linux: the key derived from the machine id); the caller should
    // encrypt the plaintext again and store the new ciphertext.
    static bool decryptBytes(const QByteArray& ciphertext, QByteArray* plaintext,
                             bool* needs_reencrypt = nullptr);

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_APPLE)
    // Tests only: where the random key is kept, and a seal with the outdated key.
    static void setKeyFilePathForTesting(const QString& path);
    static bool encryptBytesLegacyForTesting(const QByteArray& plaintext, QByteArray* ciphertext);
#endif

private:
    Q_DISABLE_COPY_MOVE(OSCrypt)
};

#endif // BASE_CRYPTO_OS_CRYPT_H
