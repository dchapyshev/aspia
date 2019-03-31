//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__QT_LOGGING_H
#define BASE__QT_LOGGING_H

#include "base/logging.h"

#include <QRect>
#include <QSize>
#include <QString>

namespace base {

// Initializes message logging for Qt.
void initQtLogging();

} // namespace base

std::ostream& operator<<(std::ostream& out, const QByteArray& qbytearray);
std::ostream& operator<<(std::ostream& out, const QPoint& qpoint);
std::ostream& operator<<(std::ostream& out, const QRect& qrect);
std::ostream& operator<<(std::ostream& out, const QSize& qsize);
std::ostream& operator<<(std::ostream& out, const QString& qstr);
std::ostream& operator<<(std::ostream& out, const QStringList& qstrlist);

#endif // BASE__QT_LOGGING_H
