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
#include <QVersionNumber>

#include "proto/common.h"

#include <google/protobuf/message_lite.h>

namespace base {

QByteArray serialize(const google::protobuf::MessageLite& message);

template <class T>
bool parse(const QByteArray& buffer, T* message)
{
    return message->ParseFromArray(buffer.data(), static_cast<int>(buffer.size()));
}

proto::Version serialize(const QVersionNumber& version);
QVersionNumber parse(const proto::Version& version);

} // namespace base

#endif // BASE_MEMORY_BYTE_ARRAY_H
