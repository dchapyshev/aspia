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

#ifndef BASE_X11_X11_UTIL_H
#define BASE_X11_X11_UTIL_H

#include <QString>

class X11Util
{
    Q_GADGET
    Q_DISABLE_COPY_MOVE(X11Util)

public:
    // Returns the X authority file the running X server uses for |user_name|, so a process launched
    // as that user can authenticate to the display. The cookie is whatever the X server was started
    // with (its -auth argument); GDM keeps it under the user runtime directory, classic setups in
    // the home directory. Returns an empty string if the user is unknown.
    static QString xauthorityForUser(const QString& user_name);
};

#endif // BASE_X11_X11_UTIL_H
