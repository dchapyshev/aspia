//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CRYPTO__DATA_ENCRYPTOR_H_
#define ASPIA_CRYPTO__DATA_ENCRYPTOR_H_

#include <QByteArray>

#include "base/macros_magic.h"

namespace aspia {

class DataEncryptor
{
public:
    // Creates a key from the password. |password| must be in UTF-8 encoding.
    static QByteArray createKey(const QByteArray& password,
                                const QByteArray& salt,
                                int rounds);

    static QByteArray encrypt(const QByteArray& source_data, const QByteArray& key);

    static bool decrypt(const QByteArray& source_data,
                        const QByteArray& key,
                        QByteArray* decrypted_data);

    static bool decrypt(const char* source_data,
                        int source_size,
                        const QByteArray& key,
                        QByteArray* decrypted_data);

private:
    DISALLOW_COPY_AND_ASSIGN(DataEncryptor);
};

} // namespace aspia

#endif // ASPIA_CRYPTO__DATA_ENCRYPTOR_H_
