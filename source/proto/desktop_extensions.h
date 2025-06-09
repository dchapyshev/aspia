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

#ifndef PROTO_DESKTOP_EXTENSIONS_H
#define PROTO_DESKTOP_EXTENSIONS_H

#include <QMetaType>

#include "proto/desktop_extensions.pb.h"

Q_DECLARE_METATYPE(proto::desktop::Resolution)
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

#endif // PROTO_DESKTOP_EXTENSIONS_H
