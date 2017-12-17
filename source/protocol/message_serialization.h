//
// PROJECT:         Aspia
// FILE:            protocol/message_serialization.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H
#define _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H

#include "protocol/io_buffer.h"
#include "base/logging.h"

#include <google/protobuf/message_lite.h>

namespace aspia {

template <class T>
static T SerializeMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();

    if (!size)
    {
        LOG(ERROR) << "Empty messages are not allowed";
        return T();
    }

    T buffer(size);
    message.SerializeWithCachedSizesToArray(buffer.data());
    return buffer;
}

template <class T>
bool ParseMessage(const IOBuffer& buffer, T& message)
{
    if (!message.ParseFromArray(buffer.data(), static_cast<int>(buffer.size())))
    {
        LOG(ERROR) << "Received message that is not a valid protocol buffer.";
        return false;
    }

    return true;
}

} // namespace aspia

#endif // _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H
