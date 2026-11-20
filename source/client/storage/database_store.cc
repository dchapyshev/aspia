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

#include "client/storage/database_store.h"

#include "base/logging.h"
#include "base/sql/sql_query.h"

//--------------------------------------------------------------------------------------------------
QString DatabaseStore::readSetting(const QString& name) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    SqlQuery query(db(), "SELECT value FROM settings WHERE name=?");
    query.addText(name);

    if (query.next() != SqlQuery::StepResult::ROW)
        return QString();

    return query.columnText(0);
}

//--------------------------------------------------------------------------------------------------
bool DatabaseStore::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(name);
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}
