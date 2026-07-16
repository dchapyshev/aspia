//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QRect>
#include <QVersionNumber>

#include <google/protobuf/message_lite.h>

#include <limits>
#include <tuple>

namespace proto::peer {
class Version;
} // namespace proto::peer

namespace proto::screen {
class Point;
class Size;
} // namespace proto::screen

namespace proto::video {
class Rect;
} // namespace proto::video

QByteArray serialize(const google::protobuf::MessageLite& message);

template <class T>
bool parse(const QByteArray& buffer, T* message)
{
    return message->ParseFromArray(buffer.data(), buffer.size());
}

proto::peer::Version serialize(const QVersionNumber& version);
QVersionNumber parse(const proto::peer::Version& version);

proto::screen::Size serialize(const QSize& size);
QSize parse(const proto::screen::Size& size);

proto::screen::Point serialize(const QPoint& point);
QPoint parse(const proto::screen::Point& point);

proto::video::Rect serialize(const QRect& rect);
QRect parse(const proto::video::Rect& rect);

//--------------------------------------------------------------------------------------------------
class SerializerImpl
{
public:
    static constexpr int kBufferCount = 8;

    const QByteArray& serialize(const google::protobuf::MessageLite& message) const
    {
        const size_t size = message.ByteSizeLong();
        if (!size || size > static_cast<size_t>(std::numeric_limits<qsizetype>::max()))
        {
            static QByteArray kEmptyBuffer;
            return kEmptyBuffer;
        }

        const qsizetype target_size = static_cast<qsizetype>(size);

        QByteArray& buffer = buffers_[buffer_index_];
        buffer_index_ = (buffer_index_ + 1) % kBufferCount;

        if (buffer.capacity() < target_size)
            buffer.reserve(target_size);

        buffer.resize(target_size);
        message.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));

        return buffer;
    }

private:
    mutable QByteArray buffers_[kBufferCount];
    mutable int buffer_index_ = 0;
};

//--------------------------------------------------------------------------------------------------
template <typename... Ts>
class Serializer
{
public:
    template <typename T>
    const T& message() const
    {
        return std::get<T>(messages_);
    }

    template <typename T>
    T& newMessage()
    {
        T& message = std::get<T>(messages_);
        message.Clear();
        return message;
    }

    template <typename T>
    const QByteArray& serialize() const
    {
        return impl_.serialize(std::get<T>(messages_));
    }

private:
    SerializerImpl impl_;
    std::tuple<Ts...> messages_;
};

//--------------------------------------------------------------------------------------------------
template <typename... Ts>
class Parser
{
public:
    // Returns a pointer to the reused message on success, or nullptr if parsing failed.
    template <typename T>
    T* parse(const QByteArray& buffer)
    {
        T& message = std::get<T>(messages_);
        if (!message.ParseFromArray(buffer.data(), buffer.size()))
            return nullptr;
        return &message;
    }

    template <typename T>
    const T& message() const
    {
        return std::get<T>(messages_);
    }

private:
    std::tuple<Ts...> messages_;
};

#endif // BASE_SERIALIZATION_H
