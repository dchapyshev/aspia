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

#ifndef RELAY_WIN_SERVICE_UTIL_H
#define RELAY_WIN_SERVICE_UTIL_H

#include <QTextStream>

namespace relay {

int startService(QTextStream& out);
int stopService(QTextStream& out);
int installService(QTextStream& out);
int removeService(QTextStream& out);

} // namespace relay

#endif // RELAY_WIN_SERVICE_UTIL_H
