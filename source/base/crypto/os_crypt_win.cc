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

#include "base/crypto/os_crypt.h"

#include "base/logging.h"

#include <qt_windows.h>
#include <wincrypt.h>

namespace base {

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::encryptString(const QString& plaintext, QByteArray* ciphertext)
{
    QByteArray plaintext_utf8 = plaintext.toUtf8();

    DATA_BLOB input;
    input.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext_utf8.data()));
    input.cbData = static_cast<DWORD>(plaintext_utf8.length());

    DATA_BLOB output;
    BOOL result = CryptProtectData(&input, L"", nullptr, nullptr, nullptr, 0, &output);
    if (!result)
    {
        PLOG(ERROR) << "Failed to encrypt";
        return false;
    }

    *ciphertext = QByteArray(reinterpret_cast<const char*>(output.pbData),
                             static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::decryptString(const QByteArray& ciphertext, QString* plaintext)
{
    DATA_BLOB input;
    input.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
    input.cbData = static_cast<DWORD>(ciphertext.length());

    DATA_BLOB output;
    BOOL result = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output);
    if (!result)
    {
        PLOG(ERROR) << "Failed to decrypt";
        return false;
    }

    QByteArray plaintext_utf8 =
        QByteArray::fromRawData(reinterpret_cast<char*>(output.pbData), output.cbData);

    *plaintext = QString::fromUtf8(plaintext_utf8);
    LocalFree(output.pbData);
    return true;
}

} // namespace base
