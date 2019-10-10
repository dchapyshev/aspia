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

#ifndef COMMON__MESSAGE_SERIALIZATION_H
#define COMMON__MESSAGE_SERIALIZATION_H

#include "base/logging.h"
#include "base/memory/byte_array.h"

#include <google/protobuf/message_lite.h>

namespace common {

static base::ByteArray serializeMessage(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return base::ByteArray();
    }

    base::ByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(buffer.data());
    return buffer;
}

template <class T>
bool parseMessage(const base::ByteArray& buffer, T* message)
{
    if (!message->ParseFromArray(buffer.data(), buffer.size()))
    {
        LOG(LS_WARNING) << "Received message that is not a valid protocol buffer";
        return false;
    }

    return true;
}

} // namespace common

#endif // COMMON__MESSAGE_SERIALIZATION_H
