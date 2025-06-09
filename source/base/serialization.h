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

#ifndef BASE_SERIALIZATION_H
#define BASE_SERIALIZATION_H

#include <QByteArray>
#include <QVersionNumber>

#include "proto/peer_common.h"

#include <google/protobuf/message_lite.h>

namespace base {

QByteArray serialize(const google::protobuf::MessageLite& message);

template <class T>
bool parse(const QByteArray& buffer, T* message)
{
    return message->ParseFromArray(buffer.data(), buffer.size());
}

proto::peer::Version serialize(const QVersionNumber& version);
QVersionNumber parse(const proto::peer::Version& version);

class SerializerImpl
{
public:
    static constexpr int kBufferCount = 8;

    const QByteArray& serialize(const google::protobuf::MessageLite& message) const
    {
        const int size = static_cast<int>(message.ByteSizeLong());
        if (!size)
        {
            static QByteArray kEmptyBuffer;
            return kEmptyBuffer;
        }

        QByteArray& buffer = buffers_[buffer_index_];
        buffer_index_ = (buffer_index_ + 1) % kBufferCount;

        if (buffer.capacity() < size)
            buffer.reserve(size);

        buffer.resize(size);
        message.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));

        return buffer;
    }

private:
    mutable QByteArray buffers_[kBufferCount];
    mutable int buffer_index_ = 0;
};

template <typename T>
class Serializer
{
public:
    const T& message() const
    {
        return message_;
    }

    T& newMessage()
    {
        message_.Clear();
        return message_;
    }

    const QByteArray& serialize() const
    {
        return impl_.serialize(message_);
    }

private:
    SerializerImpl impl_;
    T message_;
};

template <typename T>
class Parser
{
public:
    bool parse(const QByteArray& buffer)
    {
        return message_.ParseFromArray(buffer.data(), buffer.size());
    }

    const T& message() const
    {
        return message_;
    }

    const T* operator->() const
    {
        return &message_;
    }

    T* operator->()
    {
        return &message_;
    }

private:
    T message_;
};

} // namespace base

#endif // BASE_SERIALIZATION_H
