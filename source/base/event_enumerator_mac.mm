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

#include "base/event_enumerator_mac.h"

#import <Foundation/Foundation.h>
#import <OSLog/OSLog.h>

#include <algorithm>
#include <cstring>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type typeFromLevel(OSLogEntryLogLevel level)
{
    // The unified log has no dedicated "warning" level; map the two failure levels to an error and
    // everything else (debug/info/notice/undefined) to an informational entry.
    switch (level)
    {
        case OSLogEntryLogLevelError:
        case OSLogEntryLogLevelFault:
            return EventEnumerator::Type::ERR;

        default:
            return EventEnumerator::Type::INFO;
    }
}

//--------------------------------------------------------------------------------------------------
// Approximates the requested Windows-style log with a predicate over the unified log. The unified log
// has no matching split, so only "Security" (authentication/authorization processes) is narrowed; the
// other logs return the whole store.
NSPredicate* predicateForLog(const QString& log_name)
{
    if (log_name == "Security")
    {
        return [NSPredicate predicateWithFormat:@"process IN %@",
            @[ @"authd", @"opendirectoryd", @"securityd", @"sudo", @"loginwindow", @"authorizationhost" ]];
    }

    return nil;
}

//--------------------------------------------------------------------------------------------------
// The opaque cursor is the entry's Unix time (seconds, fractional) serialized as a raw double.
QByteArray cursorFromTime(double time)
{
    QByteArray cursor;
    cursor.resize(sizeof(double));
    memcpy(cursor.data(), &time, sizeof(double));
    return cursor;
}

//--------------------------------------------------------------------------------------------------
double timeFromCursor(const QByteArray& cursor)
{
    double time = 0;
    if (cursor.size() == static_cast<qsizetype>(sizeof(double)))
        memcpy(&time, cursor.constData(), sizeof(double));
    return time;
}

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumeratorMac::EventEnumeratorMac(const QString& log_name, const QByteArray& cursor,
                                       Direction direction, quint32 count)
{
    if (!count)
        return;

    @autoreleasepool
    {
        // The system-wide scope reads the whole unified log and works for a privileged (root) process.
        // localStoreAndReturnError reads the on-disk local store but needs the private
        // com.apple.private.logging.local-store entitlement, so it returns nothing for us.
        NSError* error = nil;
        OSLogStore* store = [OSLogStore storeWithScope:OSLogStoreSystem error:&error];
        if (!store)
        {
            LOG(ERROR) << "OSLogStore(system) failed: "
                       << (error ? error.localizedDescription.UTF8String : "unknown");
            return;
        }

        // NEWER reads forwards and is reversed afterwards so the page stays newest-first; OLDER reads
        // in reverse (newest-first). An empty cursor jumps to an end of the log: OLDER is the newest
        // page (from the tail), NEWER is the oldest page (from the head).
        const bool read_forward = (direction == Direction::NEWER);
        const bool has_cursor = !cursor.isEmpty();
        const double cursor_time = timeFromCursor(cursor);

        OSLogPosition* position = nil;
        if (has_cursor)
        {
            position = [store positionWithDate:[NSDate dateWithTimeIntervalSince1970:cursor_time]];
        }
        else if (!read_forward)
        {
            // A reverse enumeration needs an explicit end anchor; with a nil position it yields nothing.
            // Forward from a nil position correctly starts at the oldest retained entry.
            position = [store positionWithTimeIntervalSinceEnd:0];
        }

        if (!has_cursor)
        {
            if (read_forward)
                at_oldest_ = true;
            else
                at_newest_ = true;
        }

        OSLogEnumeratorOptions options = read_forward ? 0 : OSLogEnumeratorReverse;

        OSLogEnumerator* enumerator = [store entriesEnumeratorWithOptions:options
                                                                position:position
                                                               predicate:predicateForLog(log_name)
                                                                   error:&error];
        if (!enumerator)
        {
            LOG(ERROR) << "entriesEnumerator failed: "
                       << (error ? error.localizedDescription.UTF8String : "unknown");
            return;
        }

        QByteArray first_read;
        QByteArray last_read;
        bool hit_boundary = true;

        for (OSLogEntry* entry in enumerator)
        {
            const double entry_time = entry.date.timeIntervalSince1970;

            // positionWithDate: lands at the cursor entry itself; keep only entries strictly beyond
            // it in the read direction so the cursor page is not returned twice.
            if (has_cursor)
            {
                if (read_forward && entry_time <= cursor_time)
                    continue;
                if (!read_forward && entry_time >= cursor_time)
                    continue;
            }

            // Only actual log messages carry a level and a composed message; skip activity/signpost
            // and boundary entries.
            if (![entry isKindOfClass:[OSLogEntryLog class]])
                continue;

            OSLogEntryLog* log_entry = static_cast<OSLogEntryLog*>(entry);

            Record record;
            record.type = typeFromLevel(log_entry.level);
            record.time = static_cast<qint64>(entry_time);
            record.source = QString::fromUtf8(log_entry.process.UTF8String);
            record.description = QString::fromUtf8(log_entry.composedMessage.UTF8String);
            records_.append(std::move(record));

            const QByteArray current = cursorFromTime(entry_time);
            if (records_.size() == 1)
                first_read = current;
            last_read = current;

            if (static_cast<quint32>(records_.size()) >= count)
            {
                hit_boundary = false;
                break;
            }
        }

        if (read_forward)
        {
            std::reverse(records_.begin(), records_.end());
            first_cursor_ = last_read;   // newest entry of the page
            last_cursor_ = first_read;   // oldest entry of the page
            at_newest_ = hit_boundary;   // reading newer hit the tail
        }
        else
        {
            first_cursor_ = first_read;  // newest entry of the page
            last_cursor_ = last_read;    // oldest entry of the page
            at_oldest_ = hit_boundary;   // reading older hit the head
        }
    }
}

//--------------------------------------------------------------------------------------------------
EventEnumeratorMac::~EventEnumeratorMac() = default;

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorMac::isAtEnd() const
{
    return index_ >= records_.size();
}

//--------------------------------------------------------------------------------------------------
void EventEnumeratorMac::advance()
{
    ++index_;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type EventEnumeratorMac::type() const
{
    if (index_ < 0 || index_ >= records_.size())
        return Type::INFO;

    return records_[index_].type;
}

//--------------------------------------------------------------------------------------------------
qint64 EventEnumeratorMac::time() const
{
    if (index_ < 0 || index_ >= records_.size())
        return 0;

    return records_[index_].time;
}

//--------------------------------------------------------------------------------------------------
quint32 EventEnumeratorMac::eventId() const
{
    // The unified log has no equivalent of the Windows numeric event id.
    return 0;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorMac::source() const
{
    if (index_ < 0 || index_ >= records_.size())
        return QString();

    return records_[index_].source;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorMac::description() const
{
    if (index_ < 0 || index_ >= records_.size())
        return QString();

    return records_[index_].description;
}

//--------------------------------------------------------------------------------------------------
QByteArray EventEnumeratorMac::firstCursor() const
{
    return first_cursor_;
}

//--------------------------------------------------------------------------------------------------
QByteArray EventEnumeratorMac::lastCursor() const
{
    return last_cursor_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorMac::atNewest() const
{
    return at_newest_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorMac::atOldest() const
{
    return at_oldest_;
}
