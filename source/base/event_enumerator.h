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

#ifndef BASE_EVENT_ENUMERATOR_H
#define BASE_EVENT_ENUMERATOR_H

#include <QObject>
#include <QString>

#include <memory>

class EventEnumerator
{
    Q_GADGET

public:
    virtual ~EventEnumerator() = default;

    enum class Type { UNKNOWN, ERR, WARN, INFO, AUDIT_SUCCESS, AUDIT_FAILURE, SUCCESS };
    Q_ENUM(Type)

    static std::unique_ptr<EventEnumerator> create(
        const QString& log_name, quint32 start, quint32 count);

    virtual quint32 count() const = 0;
    virtual bool isAtEnd() const = 0;
    virtual void advance() = 0;

    virtual Type type() const = 0;
    virtual qint64 time() const = 0;
    virtual QString category() const = 0;
    virtual quint32 eventId() const = 0;
    virtual QString source() const = 0;
    virtual QString description() const = 0;

protected:
    EventEnumerator() = default;

private:
    Q_DISABLE_COPY_MOVE(EventEnumerator)
};

#endif // BASE_EVENT_ENUMERATOR_H
