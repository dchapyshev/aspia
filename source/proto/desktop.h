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

#ifndef PROTO_DESKTOP_H
#define PROTO_DESKTOP_H

#include <QMetaType>
#include <QTextStream>

#include "proto/desktop.pb.h"

Q_DECLARE_METATYPE(proto::desktop::Point)
Q_DECLARE_METATYPE(proto::desktop::Screen)
Q_DECLARE_METATYPE(proto::desktop::ScreenList)
Q_DECLARE_METATYPE(proto::desktop::PreferredSize)
Q_DECLARE_METATYPE(proto::desktop::Pause)
Q_DECLARE_METATYPE(proto::desktop::PowerControl)
Q_DECLARE_METATYPE(proto::desktop::PowerControl::Action)
Q_DECLARE_METATYPE(proto::desktop::VideoRecording)
Q_DECLARE_METATYPE(proto::desktop::VideoRecording::Action)
Q_DECLARE_METATYPE(proto::desktop::ScreenType)
Q_DECLARE_METATYPE(proto::desktop::ScreenType::Type)
Q_DECLARE_METATYPE(proto::desktop::KeyEvent)
Q_DECLARE_METATYPE(proto::desktop::TextEvent)
Q_DECLARE_METATYPE(proto::desktop::MouseEvent)
Q_DECLARE_METATYPE(proto::desktop::TouchEventPoint)
Q_DECLARE_METATYPE(proto::desktop::TouchEvent)
Q_DECLARE_METATYPE(proto::desktop::TouchEvent::TouchEventType)
Q_DECLARE_METATYPE(proto::desktop::ClipboardEvent)
Q_DECLARE_METATYPE(proto::desktop::CursorShape)
Q_DECLARE_METATYPE(proto::desktop::CursorPosition)
Q_DECLARE_METATYPE(proto::desktop::Size)
Q_DECLARE_METATYPE(proto::desktop::Rect)
Q_DECLARE_METATYPE(proto::desktop::PixelFormat)
Q_DECLARE_METATYPE(proto::desktop::VideoEncoding)
Q_DECLARE_METATYPE(proto::desktop::VideoPacketFormat)
Q_DECLARE_METATYPE(proto::desktop::VideoErrorCode)
Q_DECLARE_METATYPE(proto::desktop::VideoPacket)
Q_DECLARE_METATYPE(proto::desktop::AudioEncoding)
Q_DECLARE_METATYPE(proto::desktop::AudioPacket)
Q_DECLARE_METATYPE(proto::desktop::AudioPacket::SamplingRate)
Q_DECLARE_METATYPE(proto::desktop::AudioPacket::BytesPerSample)
Q_DECLARE_METATYPE(proto::desktop::AudioPacket::Channels)
Q_DECLARE_METATYPE(proto::desktop::Extension)
Q_DECLARE_METATYPE(proto::desktop::Capabilities)
Q_DECLARE_METATYPE(proto::desktop::Capabilities::OsType)
Q_DECLARE_METATYPE(proto::desktop::Capabilities::Flag)
Q_DECLARE_METATYPE(proto::desktop::Config)
Q_DECLARE_METATYPE(proto::desktop::HostToClient)
Q_DECLARE_METATYPE(proto::desktop::ClientToHost)

static inline QTextStream& operator<<(QTextStream& out, const proto::desktop::Size& size)
{
    return out << "proto::Size(" << size.width() << "x" << size.height() << ")";
}

static inline QTextStream& operator<<(QTextStream& out, const proto::desktop::Rect& rect)
{
    return out << "proto::Rect(" << rect.x() << " " << rect.y() << " "
               << rect.width() << "x" << rect.height() << ")";
}

#endif // PROTO_DESKTOP_H
