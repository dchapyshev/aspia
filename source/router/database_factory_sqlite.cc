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

#include "router/database_factory_sqlite.h"

#include "router/database_sqlite.h"

namespace router {

//--------------------------------------------------------------------------------------------------
DatabaseFactorySqlite::DatabaseFactorySqlite() = default;

//--------------------------------------------------------------------------------------------------
DatabaseFactorySqlite::~DatabaseFactorySqlite() = default;

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Database> DatabaseFactorySqlite::createDatabase() const
{
    return DatabaseSqlite::create();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Database> DatabaseFactorySqlite::openDatabase() const
{
    return DatabaseSqlite::open();
}

} // namespace router
