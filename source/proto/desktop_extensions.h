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

Q_DECLARE_METATYPE(proto::Resolution)
Q_DECLARE_METATYPE(proto::Point)
Q_DECLARE_METATYPE(proto::Screen)
Q_DECLARE_METATYPE(proto::ScreenList)
Q_DECLARE_METATYPE(proto::PreferredSize)
Q_DECLARE_METATYPE(proto::Pause)
Q_DECLARE_METATYPE(proto::PowerControl)
Q_DECLARE_METATYPE(proto::PowerControl::Action)
Q_DECLARE_METATYPE(proto::VideoRecording)
Q_DECLARE_METATYPE(proto::VideoRecording::Action)
Q_DECLARE_METATYPE(proto::ScreenType)
Q_DECLARE_METATYPE(proto::ScreenType::Type)

#endif // PROTO_DESKTOP_EXTENSIONS_H
