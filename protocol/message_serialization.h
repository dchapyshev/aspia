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
#include <memory>

namespace aspia {

static std::unique_ptr<IOBuffer> SerializeMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();

    if (!size)
    {
        LOG(ERROR) << "Empty messages are not allowed";
        return nullptr;
    }

    std::unique_ptr<IOBuffer> buffer(new IOBuffer(size));

    message.SerializeWithCachedSizesToArray(buffer->Data());

    return buffer;
}

template <class T>
std::unique_ptr<T> ParseMessage(const IOBuffer* buffer)
{
    std::unique_ptr<T> message(new T());

    if (!message->ParseFromArray(buffer->Data(), buffer->Size()))
    {
        LOG(ERROR) << "Received message that is not a valid protocol buffer.";
        return nullptr;
    }

    return message;
}

} // namespace aspia

#endif // _ASPIA_PROTOCOL__MESSAGE_SERIALIZATION_H
