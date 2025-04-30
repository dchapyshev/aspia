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

#include "base/gui_logging.h"

//--------------------------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const QPoint& qpoint)
{
    return out << "QPoint(" << qpoint.x() << ' ' << qpoint.y() << ')';
}

//--------------------------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const QRect& qrect)
{
    return out << "QRect("
               << qrect.left()  << ' ' << qrect.top() << ' '
               << qrect.width() << 'x' << qrect.height()
               << ')';
}

//--------------------------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const QSize& qsize)
{
    return out << "QSize(" << qsize.width() << ' ' << qsize.height() << ')';
}
