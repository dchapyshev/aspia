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

#include "router/email_queue.h"

#include <QDir>
#include <QFileInfo>

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool ensureSchema(SqlDatabase& db)
{
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"emails\" ("
                 "\"id\" INTEGER PRIMARY KEY,"
                 "\"time\" INTEGER NOT NULL,"
                 "\"recipient\" TEXT NOT NULL,"
                 "\"subject\" TEXT NOT NULL,"
                 "\"text\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create the email queue table:" << db.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QString EmailQueue::filePath()
{
    return BasePaths::appDataDir() + "/email_queue.db3";
}

//--------------------------------------------------------------------------------------------------
bool EmailQueue::open(const QString& file_path)
{
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    if (!QDir().mkpath(QFileInfo(file_path).absolutePath()))
    {
        LOG(ERROR) << "Unable to create directory for email queue";
        return false;
    }

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "email queue:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA journal_mode = WAL"))
        LOG(WARNING) << "Unable to enable WAL journal mode:" << db_.lastError();

    if (!ensureSchema(db_))
    {
        db_.close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void EmailQueue::close()
{
    db_.close();
}

//--------------------------------------------------------------------------------------------------
bool EmailQueue::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
bool EmailQueue::add(const QString& to, const QString& subject, const QString& text)
{
    if (!isValid())
    {
        LOG(ERROR) << "Email queue is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO emails (time, recipient, subject, text) VALUES (?, ?, ?, ?)");
    query.addInt64(secondsSinceEpoch());
    query.addText(to);
    query.addText(subject);
    query.addText(text);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool EmailQueue::front(Record* record) const
{
    CHECK(record);

    if (!isValid())
    {
        LOG(ERROR) << "Email queue is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, time, recipient, subject, text FROM emails ORDER BY id LIMIT 1");

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();

    if (step != SqlQuery::StepResult::ROW)
        return false;

    record->id = query.columnInt64(0);
    record->time = query.columnInt64(1);
    record->to = query.columnText(2);
    record->subject = query.columnText(3);
    record->text = query.columnText(4);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool EmailQueue::remove(qint64 id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Email queue is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM emails WHERE id = ?");
    query.addInt64(id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
