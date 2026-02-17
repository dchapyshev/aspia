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

//--------------------------------------------------------------------------------------------------
proto::desktop::Rect serialize(const QRect& rect)
{
    proto::desktop::Rect result;

    result.set_x(rect.x());
    result.set_y(rect.y());
    result.set_width(rect.width());
    result.set_height(rect.height());

    return result;
}

//--------------------------------------------------------------------------------------------------
QRect parse(const proto::desktop::Rect& rect)
{
    return QRect(QPoint(rect.x(), rect.y()), QSize(rect.width(), rect.height()));
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Point serialize(const QPoint& point)
{
    proto::desktop::Point result;

    result.set_x(point.x());
    result.set_y(point.y());

    return result;
}

//--------------------------------------------------------------------------------------------------
QPoint parse(const proto::desktop::Point& point)
{
    return QPoint(point.x(), point.y());
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Size serialize(const QSize& size)
{
    proto::desktop::Size result;

    result.set_width(size.width());
    result.set_height(size.height());

    return result;
}

//--------------------------------------------------------------------------------------------------
QSize parse(const proto::desktop::Size& size)
{
    return QSize(size.width(), size.height());
}

//--------------------------------------------------------------------------------------------------
proto::desktop::PixelFormat serialize(const PixelFormat& format)
{
    proto::desktop::PixelFormat result;

    result.set_bits_per_pixel(format.bitsPerPixel());
    result.set_red_max(format.redMax());
    result.set_green_max(format.greenMax());
    result.set_blue_max(format.blueMax());
    result.set_red_shift(format.redShift());
    result.set_green_shift(format.greenShift());
    result.set_blue_shift(format.blueShift());

    return result;
}

//--------------------------------------------------------------------------------------------------
PixelFormat parse(const proto::desktop::PixelFormat& format)
{
    return base::PixelFormat(
        static_cast<quint8>(format.bits_per_pixel()),
        static_cast<quint16>(format.red_max()),
        static_cast<quint16>(format.green_max()),
        static_cast<quint16>(format.blue_max()),
        static_cast<quint8>(format.red_shift()),
        static_cast<quint8>(format.green_shift()),
        static_cast<quint8>(format.blue_shift()));
}

} // namespace base
