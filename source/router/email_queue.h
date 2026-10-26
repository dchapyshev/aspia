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

#ifndef ROUTER_EMAIL_QUEUE_H
#define ROUTER_EMAIL_QUEUE_H

#include "base/sql/sql_database.h"

// The emails waiting to be sent, in the order they were added. The queue survives a restart.
class EmailQueue
{
public:
    struct Record
    {
        qint64 id = 0;
        qint64 time = 0;
        QString to;
        QString subject;
        QString text;
    };

    EmailQueue() = default;
    ~EmailQueue() = default;

    static QString filePath();

    bool open(const QString& file_path);
    void close();
    bool isValid() const;

    bool add(const QString& to, const QString& subject, const QString& text);

    // The oldest email. False when the queue is empty or cannot be read.
    bool front(Record* record) const;

    bool remove(qint64 id);

private:
    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(EmailQueue)
};

#endif // ROUTER_EMAIL_QUEUE_H
