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

#include "system_info/serializer/serializer.h"

namespace aspia {

Serializer::Serializer(QObject* parent, const QString& uuid)
    : QObject(parent),
      category_uuid_(uuid)
{
    // Nothing
}

void Serializer::onReady(const google::protobuf::MessageLite& message)
{
    QByteArray buffer;

    size_t size = message.ByteSizeLong();
    if (size)
    {
        buffer.resize(size);
        message.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    }

    emit replyReady(requestUuid(), buffer);
    emit finished();
}

} // namespace aspia
