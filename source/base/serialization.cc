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

#include "base/serialization.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
QByteArray serialize(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
        return QByteArray();

    QByteArray buffer;
    buffer.resize(static_cast<QByteArray::size_type>(size));

    message.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    return buffer;
}

//--------------------------------------------------------------------------------------------------
proto::peer::Version serialize(const QVersionNumber& version)
{
    if (version.segmentCount() != 3)
    {
        LOG(ERROR) << "Invalid version segments count:" << version.segmentCount();
        return proto::peer::Version();
    }

    proto::peer::Version proto_version;
    proto_version.set_major(static_cast<quint32>(version.majorVersion()));
    proto_version.set_minor(static_cast<quint32>(version.minorVersion()));
    proto_version.set_patch(static_cast<quint32>(version.microVersion()));

    return proto_version;
}

//--------------------------------------------------------------------------------------------------
QVersionNumber parse(const proto::peer::Version& version)
{
    return QVersionNumber({ static_cast<int>(version.major()),
                            static_cast<int>(version.minor()),
                            static_cast<int>(version.patch()) });
}

} // namespace base
