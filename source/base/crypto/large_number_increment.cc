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

static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "Only little-endian platforms are supported");

//--------------------------------------------------------------------------------------------------
void largeNumberIncrement(quint8* buffer, size_t buffer_size)
{
    DCHECK(buffer);
    DCHECK(buffer_size);

    size_t n = buffer_size;
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

//--------------------------------------------------------------------------------------------------
void largeNumberIncrement(QByteArray* buffer)
{
    DCHECK(buffer);
    largeNumberIncrement(reinterpret_cast<quint8*>(buffer->data()), buffer->size());
}
