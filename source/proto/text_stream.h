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

#ifndef PROTO_TEXT_STREAM_H
#define PROTO_TEXT_STREAM_H

#include <QTextStream>

#include "proto/desktop.pb.h"
#include "proto/desktop_internal.pb.h"
#include "proto/host_internal.pb.h"
#include "proto/router.pb.h"

QTextStream& operator<<(QTextStream& out, const proto::internal::RouterState& state);
QTextStream& operator<<(QTextStream& out, proto::router::SessionType session_type);
QTextStream& operator<<(QTextStream& out, proto::internal::DesktopControl::Action action);
QTextStream& operator<<(QTextStream& out, proto::desktop::AudioEncoding encoding);
QTextStream& operator<<(QTextStream& out, proto::desktop::VideoEncoding encoding);
QTextStream& operator<<(QTextStream& out, proto::desktop::VideoErrorCode error_code);
QTextStream& operator<<(QTextStream& out, proto::peer::SessionType session_type);

#endif // PROTO_TEXT_STREAM_H
