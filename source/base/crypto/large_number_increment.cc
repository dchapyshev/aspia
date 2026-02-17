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

#include "base/crypto/large_number_increment.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
void largeNumberIncrement(quint8* buffer, size_t buffer_size)
{
    DCHECK(buffer);
    DCHECK(buffer_size);

    const union
    {
        long one;
        char little;
    } is_endian = { 1 };

    if (is_endian.little || (reinterpret_cast<size_t>(buffer) % sizeof(size_t)) != 0)
    {
        quint32 n = static_cast<quint32>(buffer_size);
        quint32 c = 1;

        do
        {
            --n;
            c += buffer[n];
            buffer[n] = static_cast<quint8>(c);
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

//--------------------------------------------------------------------------------------------------
void largeNumberIncrement(QByteArray* buffer)
{
    DCHECK(buffer);
    largeNumberIncrement(reinterpret_cast<quint8*>(buffer->data()), buffer->size());
}

} // namespace base
