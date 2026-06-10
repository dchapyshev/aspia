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

#ifndef COMMON_DESKTOP_SESSION_TYPE_H
#define COMMON_DESKTOP_SESSION_TYPE_H

#include <QIcon>
#include <QString>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

QString sessionName(proto::peer::SessionType session_type);
QIcon sessionIcon(proto::peer::SessionType session_type);

#endif // COMMON_DESKTOP_SESSION_TYPE_H
