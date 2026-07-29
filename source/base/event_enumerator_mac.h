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

#ifndef BASE_EVENT_ENUMERATOR_MAC_H
#define BASE_EVENT_ENUMERATOR_MAC_H

#include "base/event_enumerator.h"

#include <QList>

class EventEnumeratorMac final : public EventEnumerator
{
public:
    EventEnumeratorMac(std::string_view log_name, std::string_view cursor, Direction direction,
                       quint32 count);
    ~EventEnumeratorMac() final;

    // EventEnumerator implementation.
    bool isAtEnd() const final;
    void advance() final;
    Type type() const final;
    qint64 time() const final;
    quint32 eventId() const final;
    std::string source() const final;
    std::string description() const final;
    std::string firstCursor() const final;
    std::string lastCursor() const final;
    bool atNewest() const final;
    bool atOldest() const final;

private:
    struct Record
    {
        Type type = Type::INFO;
        qint64 time = 0;
        std::string source;
        std::string description;
    };

    // The page is read into memory in the constructor (a NEWER page is read oldest-first and then
    // reversed), so the OSLogStore enumerator does not outlive the constructor.
    QList<Record> records_;
    qsizetype index_ = 0;
    std::string first_cursor_;
    std::string last_cursor_;
    bool at_newest_ = false;
    bool at_oldest_ = false;

    Q_DISABLE_COPY_MOVE(EventEnumeratorMac)
};

#endif // BASE_EVENT_ENUMERATOR_MAC_H
