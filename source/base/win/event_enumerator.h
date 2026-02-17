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

#ifndef BASE_WIN_EVENT_ENUMERATOR_H
#define BASE_WIN_EVENT_ENUMERATOR_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include "base/win/scoped_object.h"

namespace base {

class EventEnumerator
{
    Q_GADGET

public:
    EventEnumerator(const QString& log_name, quint32 start, quint32 count);
    ~EventEnumerator();

    quint32 count() const;
    bool isAtEnd() const;
    void advance();

    enum class Type { UNKNOWN, ERR, WARN, INFO, AUDIT_SUCCESS, AUDIT_FAILURE, SUCCESS };
    Q_ENUM(Type)

    Type type() const;
    qint64 time() const;
    QString category() const;
    quint32 eventId() const;
    QString source() const;
    QString description() const;

private:
    EVENTLOGRECORD* record() const;

    QString log_name_;
    ScopedEventLog event_log_;
    quint32 records_count_ = 0;
    int end_record_ = 0;

    mutable int current_pos_ = 0;
    mutable QByteArray record_buffer_;

    Q_DISABLE_COPY(EventEnumerator)
};

} // namespace base

#endif // BASE_WIN_EVENT_ENUMERATOR_H
