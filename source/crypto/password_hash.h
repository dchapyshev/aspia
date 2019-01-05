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

#ifndef ASPIA_CRYPTO__PASSWORD_HASH_H
#define ASPIA_CRYPTO__PASSWORD_HASH_H

#include <QByteArray>

#include "base/macros_magic.h"

namespace aspia {

class PasswordHash
{
public:
    enum Type { SCRYPT };

    static const size_t kBitsPerByte = 8;
    static const size_t kBitsSize = 256;
    static const size_t kBytesSize = kBitsSize / kBitsPerByte;

    static QByteArray hash(Type type, const QByteArray& password, const QByteArray& salt);

private:
    DISALLOW_COPY_AND_ASSIGN(PasswordHash);
};

} // namespace aspia

#endif // ASPIA_CRYPTO__PASSWORD_HASH_H
