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

#include "base/sql/sql_transaction.h"

#include "base/sql/sql_database.h"

//--------------------------------------------------------------------------------------------------
SqlTransaction::SqlTransaction(SqlDatabase& db)
    : db_(db)
{
    // Nothing is opened until begin() is called.
}

//--------------------------------------------------------------------------------------------------
SqlTransaction::~SqlTransaction()
{
    if (active_)
        db_.rollbackTransaction();
}

//--------------------------------------------------------------------------------------------------
bool SqlTransaction::begin()
{
    if (active_)
        return false;

    active_ = db_.beginTransaction();
    return active_;
}

//--------------------------------------------------------------------------------------------------
bool SqlTransaction::commit()
{
    if (!active_)
        return false;

    active_ = false;

    if (db_.commitTransaction())
        return true;

    // A failed COMMIT (SQLITE_BUSY, disk full) may leave the transaction open, and the connection
    // would then reject every following BEGIN. Roll it back explicitly; if sqlite has already
    // rolled it back automatically, the extra ROLLBACK fails without doing any harm.
    db_.rollbackTransaction();
    return false;
}
