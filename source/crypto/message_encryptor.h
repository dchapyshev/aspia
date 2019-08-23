//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CRYPTO__MESSAGE_ENCRYPTOR_H
#define CRYPTO__MESSAGE_ENCRYPTOR_H

#include <cstdint>

namespace crypto {

class MessageEncryptor
{
public:
    virtual ~MessageEncryptor() = default;

    virtual size_t encryptedDataSize(size_t in_size) = 0;
    virtual bool encrypt(const uint8_t* in, size_t in_size, uint8_t* out) = 0;
};

} // namespace crypto

#endif // CRYPTO__MESSAGE_ENCRYPTOR_H
