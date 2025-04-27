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

#include "proto/desktop.pb.h"

Q_DECLARE_METATYPE(proto::KeyEvent)
Q_DECLARE_METATYPE(proto::TextEvent)
Q_DECLARE_METATYPE(proto::MouseEvent)
Q_DECLARE_METATYPE(proto::TouchEventPoint)
Q_DECLARE_METATYPE(proto::TouchEvent)
Q_DECLARE_METATYPE(proto::TouchEvent::TouchEventType)
Q_DECLARE_METATYPE(proto::ClipboardEvent)
Q_DECLARE_METATYPE(proto::CursorShape)
Q_DECLARE_METATYPE(proto::CursorPosition)
Q_DECLARE_METATYPE(proto::Size)
Q_DECLARE_METATYPE(proto::Rect)
Q_DECLARE_METATYPE(proto::PixelFormat)
Q_DECLARE_METATYPE(proto::VideoEncoding)
Q_DECLARE_METATYPE(proto::VideoPacketFormat)
Q_DECLARE_METATYPE(proto::VideoErrorCode)
Q_DECLARE_METATYPE(proto::VideoPacket)
Q_DECLARE_METATYPE(proto::AudioEncoding)
Q_DECLARE_METATYPE(proto::AudioPacket)
Q_DECLARE_METATYPE(proto::AudioPacket::SamplingRate)
Q_DECLARE_METATYPE(proto::AudioPacket::BytesPerSample)
Q_DECLARE_METATYPE(proto::AudioPacket::Channels)
Q_DECLARE_METATYPE(proto::DesktopExtension)
Q_DECLARE_METATYPE(proto::DesktopCapabilities)
Q_DECLARE_METATYPE(proto::DesktopCapabilities::OsType)
Q_DECLARE_METATYPE(proto::DesktopCapabilities::Flag)
Q_DECLARE_METATYPE(proto::DesktopConfig)
Q_DECLARE_METATYPE(proto::HostToClient)
Q_DECLARE_METATYPE(proto::ClientToHost)

#endif // PROTO_DESKTOP_H
