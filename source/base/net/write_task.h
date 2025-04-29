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

#ifndef BASE_NET_WRITE_TASK_H
#define BASE_NET_WRITE_TASK_H

#include <QByteArray>

#include <queue>

namespace base {

class WriteTask
{
public:
    enum class Type { SERVICE_DATA, USER_DATA };

    WriteTask(Type type, uint8_t channel_id, QByteArray&& data)
        : type_(type),
          channel_id_(channel_id),
          data_(std::move(data))
    {
        // Nothing
    }

    WriteTask(const WriteTask& other) = default;
    WriteTask& operator=(const WriteTask& other) = default;

    Type type() const { return type_; }
    uint8_t channelId() const { return channel_id_; }
    const QByteArray& data() const { return data_; }
    QByteArray& data() { return data_; }

private:
    Type type_;
    uint8_t channel_id_;
    QByteArray data_;
};

using WriteQueue = std::queue<WriteTask>;

} // namespace base

#endif // BASE_NET_WRITE_TASK_H
