//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CRYPTO_MESSAGE_DECRYPTOR_FAKE_H
#define BASE_CRYPTO_MESSAGE_DECRYPTOR_FAKE_H

#include "base/macros_magic.h"
#include "base/crypto/message_decryptor.h"

namespace base {

class MessageDecryptorFake : public MessageDecryptor
{
public:
    MessageDecryptorFake();
    ~MessageDecryptorFake() override;

    // MessageDecryptor implementation.
    size_t decryptedDataSize(size_t in_size) override;
    bool decrypt(const void* in, size_t in_size, void* out) override;

private:
    DISALLOW_COPY_AND_ASSIGN(MessageDecryptorFake);
};

} // namespace base

#endif // BASE_CRYPTO_MESSAGE_DECRYPTOR_FAKE_H
