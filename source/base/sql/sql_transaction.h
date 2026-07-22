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

#ifndef BASE_SQL_SQL_TRANSACTION_H
#define BASE_SQL_SQL_TRANSACTION_H

#include "base/sql/sql_database.h"

// Scopes a database transaction to a C++ block. begin() opens it; commit() finalizes it. If the
// object is destroyed after a successful begin() without a commit() - an early return, a failed
// step, anything - the transaction is rolled back. This removes the per-error-path rollback
// boilerplate that explicit BEGIN/COMMIT handling needs.
class SqlTransaction final
{
public:
    using Mode = SqlDatabase::TransactionMode;

    explicit SqlTransaction(SqlDatabase& db);
    ~SqlTransaction();

    bool begin(Mode mode = Mode::DEFERRED);
    bool commit();

    bool isActive() const { return active_; }

private:
    SqlDatabase& db_;
    bool active_ = false;

    Q_DISABLE_COPY_MOVE(SqlTransaction)
};

#endif // BASE_SQL_SQL_TRANSACTION_H
