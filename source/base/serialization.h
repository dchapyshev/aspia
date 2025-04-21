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

#ifndef BASE_MEMORY_BYTE_ARRAY_H
#define BASE_MEMORY_BYTE_ARRAY_H

#include <QByteArray>
#include <google/protobuf/message_lite.h>

namespace base {

static inline QByteArray serialize(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
        return QByteArray();

    QByteArray buffer;
    buffer.resize(static_cast<QByteArray::size_type>(size));

    message.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    return buffer;
}

template <class T>
bool parse(const QByteArray& buffer, T* message)
{
    return message->ParseFromArray(buffer.data(), static_cast<int>(buffer.size()));
}

} // namespace base

#endif // BASE_MEMORY_BYTE_ARRAY_H
