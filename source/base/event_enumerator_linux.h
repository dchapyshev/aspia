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

#ifndef BASE_EVENT_ENUMERATOR_LINUX_H
#define BASE_EVENT_ENUMERATOR_LINUX_H

#include "base/event_enumerator.h"

struct sd_journal;

class EventEnumeratorLinux final : public EventEnumerator
{
public:
    EventEnumeratorLinux(const QString& log_name, quint32 start, quint32 count);
    ~EventEnumeratorLinux() final;

    // EventEnumerator implementation.
    quint32 count() const final;
    bool isAtEnd() const final;
    void advance() final;
    Type type() const final;
    qint64 time() const final;
    QString category() const final;
    quint32 eventId() const final;
    QString source() const final;
    QString description() const final;

private:
    QString readField(const char* field) const;

    sd_journal* journal_ = nullptr;
    quint32 records_count_ = 0;
    int remaining_ = 0;
    bool at_end_ = false;

    Q_DISABLE_COPY_MOVE(EventEnumeratorLinux)
};

#endif // BASE_EVENT_ENUMERATOR_LINUX_H
