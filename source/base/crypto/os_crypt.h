//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"

#include <string>

namespace base {

class OSCrypt
{
public:
    // Encrypt a std::u16string_view. The output (second argument) is really an array of bytes, but
    // we're passing it back as a std::string.
    static bool encryptString16(std::u16string_view plaintext, std::string* ciphertext);

    // Decrypt an array of bytes obtained with encryptString16 back into a std::u16string. Note
    // that the input (first argument) is a std::string, so you need to first get your (binary)
    // data into a string.
    static bool decryptString16(std::string_view ciphertext, std::u16string* plaintext);

    // Encrypt a string.
    static bool encryptString(std::string_view plaintext, std::string* ciphertext);

    // Decrypt an array of bytes obtained with EnctryptString back into a string. Note that the
    // input (first argument) is a std::string, so you need to first get your (binary) data into a
    // string.
    static bool decryptString(std::string_view ciphertext, std::string* plaintext);

private:
    DISALLOW_COPY_AND_ASSIGN(OSCrypt);
};

} // namespace base

#endif // BASE_CRYPTO_OS_CRYPT_H
