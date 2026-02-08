//
// SmartCafe Project
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

#ifndef BASE_PEER_HOST_ID_H
#define BASE_PEER_HOST_ID_H

#include <QMetaType>
#include <QString>

namespace base {

using HostId = unsigned long long;

extern const HostId kInvalidHostId;

// Checks if a string is a host ID.
bool isHostId(const QString& str);

// Converts a string to a host ID.
// If the string is empty or the conversion failed, then |kInvalidHostId| is returned.
HostId stringToHostId(const QString& str);

// Converts a host ID to a string.
// If the host ID is |kInvalidHostId|, an empty string is returned.
QString hostIdToString(HostId host_id);

} // namespace base

Q_DECLARE_METATYPE(base::HostId)

#endif // BASE_PEER_HOST_ID_H
