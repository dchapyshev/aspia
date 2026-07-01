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

#ifndef BASE_EVENT_ENUMERATOR_WIN_H
#define BASE_EVENT_ENUMERATOR_WIN_H

#include "base/event_enumerator.h"

#include <QByteArray>

#include "base/win/scoped_object.h"

class EventEnumeratorWin final : public EventEnumerator
{
public:
    EventEnumeratorWin(const QString& log_name, const QByteArray& cursor, Direction direction,
                       quint32 count);
    ~EventEnumeratorWin() final;

    // EventEnumerator implementation.
    bool isAtEnd() const final;
    void advance() final;
    Type type() const final;
    qint64 time() const final;
    quint32 eventId() const final;
    QString source() const final;
    QString description() const final;
    QByteArray firstCursor() const final;
    QByteArray lastCursor() const final;
    bool atNewest() const final;
    bool atOldest() const final;

private:
    bool fetchNext() const;
    bool renderSystem() const;
    QString eventDataString() const;

    QString log_name_;
    ScopedEvtHandle query_;
    ScopedEvtHandle render_context_;

    // The opaque cursor encodes the record offset from the newest record; |start_| is the offset of
    // this page's newest record.
    quint32 start_ = 0;
    quint32 count_ = 0;

    mutable int remaining_ = 0;
    mutable quint32 read_count_ = 0;
    bool at_oldest_hint_ = false;
    mutable bool event_ready_ = false;
    mutable ScopedEvtHandle event_;
    mutable QByteArray values_buffer_;

    Q_DISABLE_COPY_MOVE(EventEnumeratorWin)
};

#endif // BASE_EVENT_ENUMERATOR_WIN_H
