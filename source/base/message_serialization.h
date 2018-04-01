//
// PROJECT:         Aspia
// FILE:            base/message_serialization.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_SERIALIZATION_H
#define _ASPIA_BASE__MESSAGE_SERIALIZATION_H

#include <QDebug>
#include <QByteArray>

#include <google/protobuf/message_lite.h>

namespace aspia {

static QByteArray SerializeMessage(const google::protobuf::MessageLite& message)
{
    uint32_t size = message.ByteSize();
    if (!size)
    {
        qWarning("Empty messages are not allowed");
        return QByteArray();
    }

    QByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    return buffer;
}

template <class T>
bool ParseMessage(const QByteArray& buffer, T& message)
{
    if (!message.ParseFromArray(buffer, static_cast<int>(buffer.size())))
    {
        qWarning("Received message that is not a valid protocol buffer");
        return false;
    }

    return true;
}

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_SERIALIZATION_H
