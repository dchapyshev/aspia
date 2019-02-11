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

#include "crypto/large_number_increment.h"

namespace crypto {

void largeNumberIncrement(uint8_t* buffer, size_t buffer_size)
{
    const union
    {
        long one;
        char little;
    } is_endian = { 1 };

    if (is_endian.little || (reinterpret_cast<size_t>(buffer) % sizeof(size_t)) != 0)
    {
        uint32_t n = buffer_size;
        uint32_t c = 1;

        do
        {
            --n;
            c += buffer[n];
            buffer[n] = static_cast<uint8_t>(c);
            c >>= 8;
        }
        while (n);
    }
    else
    {
        size_t* data = reinterpret_cast<size_t*>(buffer);
        size_t n = buffer_size / sizeof(size_t);
        size_t c = 1;

        do
        {
            --n;
            size_t d = data[n] += c;
            c = ((d - c) & ~d) >> (sizeof(size_t) * 8 - 1);
        }
        while (n);
    }
}

void largeNumberIncrement(QByteArray* buffer)
{
    largeNumberIncrement(reinterpret_cast<uint8_t*>(buffer->data()), buffer->size());
}

} // namespace crypto
