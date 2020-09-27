//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef RELAY__DATABASE_SQLITE_H
#define RELAY__DATABASE_SQLITE_H

#include "base/macros_magic.h"
#include "relay/database.h"

namespace relay {

class DatabaseSqlite : public Database
{
public:
    DatabaseSqlite();
    ~DatabaseSqlite();

    // Database implementation.
    bool addSession(const Session& session) override;
    bool removeSession(int64_t entry_id) override;
    SessionList sessions(int64_t limit) override;

private:
    DISALLOW_COPY_AND_ASSIGN(DatabaseSqlite);
};

} // namespace relay

#endif // RELAY__DATABASE_SQLITE_H
