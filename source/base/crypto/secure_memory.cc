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

#include "base/crypto/secure_memory.h"

#include <openssl/crypto.h>

namespace base {

//--------------------------------------------------------------------------------------------------
void memZero(void* data, size_t data_size)
{
    if (data && data_size)
        OPENSSL_cleanse(data, data_size);
}

//--------------------------------------------------------------------------------------------------
void memZero(std::string* str)
{
    if (!str)
        return;

    memZero(str->data(), str->length() * sizeof(char));
}

//--------------------------------------------------------------------------------------------------
void memZero(std::u16string* str)
{
    if (!str)
        return;

    memZero(str->data(), str->length() * sizeof(char16_t));
}

//--------------------------------------------------------------------------------------------------
void memZero(QByteArray* str)
{
    if (!str)
        return;

    memZero(str->data(), str->size());
}

} // namespace base
