//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__MESSAGE_SERIALIZATION_H_
#define ASPIA_BASE__MESSAGE_SERIALIZATION_H_

#include <QByteArray>

#include <google/protobuf/message_lite.h>

#include "base/logging.h"

namespace aspia {

static QByteArray serializeMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return QByteArray();
    }

    QByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    return buffer;
}

template <class T>
bool parseMessage(const QByteArray& buffer, T& message)
{
    if (!message.ParseFromArray(buffer.constData(), buffer.size()))
    {
        LOG(LS_WARNING) << "Received message that is not a valid protocol buffer";
        return false;
    }

    return true;
}

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_SERIALIZATION_H_
