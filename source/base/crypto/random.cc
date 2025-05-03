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

#include "base/crypto/random.h"

#include "base/logging.h"

#include <openssl/rand.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <class ContainerT>
ContainerT generateBuffer(size_t size)
{
    ContainerT random_buffer;
    random_buffer.resize(static_cast<ContainerT::size_type>(size));

    bool result = Random::fillBuffer(random_buffer.data(), random_buffer.size());
    CHECK(result);

    return random_buffer;
}

//--------------------------------------------------------------------------------------------------
template <typename NumberT>
NumberT generateNumber()
{
    NumberT ret;

    bool result = Random::fillBuffer(&ret, sizeof(ret));
    CHECK(result);

    return ret;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool Random::fillBuffer(void* buffer, size_t size)
{
    if (!buffer || !size)
        return false;

    return RAND_bytes(reinterpret_cast<quint8*>(buffer), static_cast<int>(size)) > 0;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray Random::byteArray(size_t size)
{
    return generateBuffer<QByteArray>(size);
}

//--------------------------------------------------------------------------------------------------
// static
std::string Random::string(size_t size)
{
    return generateBuffer<std::string>(size);
}

//--------------------------------------------------------------------------------------------------
// static
quint32 Random::number32()
{
    return generateNumber<quint32>();
}

//--------------------------------------------------------------------------------------------------
// static
quint64 Random::number64()
{
    return generateNumber<quint64>();
}

} // namespace base
