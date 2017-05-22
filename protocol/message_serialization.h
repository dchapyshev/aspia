//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/message_serialization.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H
#define _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H

#include "base/io_buffer.h"
#include "base/logging.h"

#include <google/protobuf/message_lite.h>

namespace aspia {

static IOBuffer SerializeMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();

    if (!size)
    {
        LOG(ERROR) << "Empty messages are not allowed";
        return IOBuffer();
    }

    IOBuffer buffer(size);

    message.SerializeWithCachedSizesToArray(buffer.Data());

    return buffer;
}

template <class T>
bool ParseMessage(const IOBuffer& buffer, T& message)
{
    if (!message.ParseFromArray(buffer.Data(), static_cast<int>(buffer.Size())))
    {
        LOG(ERROR) << "Received message that is not a valid protocol buffer.";
        return false;
    }

    return true;
}

} // namespace aspia

#endif // _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H
