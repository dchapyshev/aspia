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

#ifndef ROUTER_DATABASE_STORE_H
#define ROUTER_DATABASE_STORE_H

#include "router/database.h"

class DatabaseStore
{
protected:
    explicit DatabaseStore(Database& db)
        : database_(db)
    {
        // Nothing
    }

    SqlDatabase& db() const { return database_.db_; }
    bool isValid() const { return database_.isValid(); }

    // Every list query has to name the page it wants. Whatever comes out of one goes into a single
    // reply, and a reply the channel cannot carry is not sent at all but ends the session, so the
    // size of an answer must never follow the size of the database. A request that names no page
    // has a count of zero and is refused along with one that asks for more than the cap.
    static bool isPageValid(qint64 offset, qint64 count, int max_count)
    {
        return offset >= 0 && count > 0 && count <= max_count;
    }

    Database& database_;
};

#endif // ROUTER_DATABASE_STORE_H
