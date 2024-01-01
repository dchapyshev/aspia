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

#include "base/memory/serializer.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Serializer::Serializer()
{
    for (size_t i = 0; i < kPoolSize; ++i)
        pool_[i].reserve(kBufferSize);
}

//--------------------------------------------------------------------------------------------------
Serializer::~Serializer() = default;

//--------------------------------------------------------------------------------------------------
base::ByteArray Serializer::serialize(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
        return base::ByteArray();

    base::ByteArray buffer;

    if (free_)
    {
        buffer = std::move(pool_[free_ - 1]);

        if (buffer.capacity() < size)
            buffer.reserve(size);

        --free_;
    }

    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(buffer.data());
    return buffer;
}

//--------------------------------------------------------------------------------------------------
void Serializer::addBuffer(base::ByteArray&& buffer)
{
    if (free_ < kPoolSize)
    {
        pool_[free_] = std::move(buffer);
        ++free_;
    }
}

} // namespace base
