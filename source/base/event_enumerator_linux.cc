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

#include "base/event_enumerator_linux.h"

#include "base/logging.h"
#include "base/linux/libsystemd.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type typeFromPriority(int priority)
{
    // syslog priorities: 0=emerg, 1=alert, 2=crit, 3=err, 4=warning, 5=notice, 6=info, 7=debug.
    if (priority <= 3)
        return EventEnumerator::Type::ERR;
    if (priority == 4)
        return EventEnumerator::Type::WARN;

    return EventEnumerator::Type::INFO;
}

//--------------------------------------------------------------------------------------------------
// Restricts the journal to the entries that approximate the requested Windows-style log. The
// "Application" log keeps the whole journal. Matches on the same field are OR-ed by journald.
void addLogMatches(sd_journal* journal, const QString& log_name)
{
    if (log_name == "Security")
    {
        // Authentication and authorization (syslog auth + authpriv facilities).
        LibSystemd::journalAddMatch(journal, "SYSLOG_FACILITY=4", 0);
        LibSystemd::journalAddMatch(journal, "SYSLOG_FACILITY=10", 0);
    }
    else if (log_name == "System")
    {
        // Kernel and system daemon messages.
        LibSystemd::journalAddMatch(journal, "SYSLOG_FACILITY=0", 0);
        LibSystemd::journalAddMatch(journal, "SYSLOG_FACILITY=3", 0);
    }
}

//--------------------------------------------------------------------------------------------------
// Reads a journal field of the current entry without the "FIELD=" prefix.
QString readField(sd_journal* journal, const char* field)
{
    const void* data = nullptr;
    size_t length = 0;
    if (LibSystemd::journalGetData(journal, field, &data, &length) < 0)
        return QString();

    const char* chars = static_cast<const char*>(data);
    const char* separator = static_cast<const char*>(memchr(chars, '=', length));
    if (!separator)
        return QString();

    const qsizetype offset = (separator - chars) + 1;
    return QString::fromUtf8(chars + offset, static_cast<qsizetype>(length) - offset);
}

//--------------------------------------------------------------------------------------------------
// Returns the opaque cursor of the current entry.
QByteArray currentCursor(sd_journal* journal)
{
    char* cursor = nullptr;
    if (LibSystemd::journalGetCursor(journal, &cursor) < 0 || !cursor)
        return QByteArray();

    QByteArray result(cursor);
    free(cursor);
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumeratorLinux::EventEnumeratorLinux(const QString& log_name, const QByteArray& cursor,
                                           Direction direction, quint32 count)
{
    if (!count)
        return;

    sd_journal* journal = nullptr;
    int ret = LibSystemd::journalOpen(&journal);
    if (ret < 0)
    {
        LOG(ERROR) << "journalOpen failed:" << ret;
        return;
    }

    addLogMatches(journal, log_name);

    // NEWER reads forwards (next) and is reversed afterwards so the page stays newest-first; OLDER
    // reads backwards (previous). An empty cursor jumps to an end of the log: OLDER is the newest
    // page (from the tail), NEWER is the oldest page (from the head).
    const bool read_forward = (direction == Direction::NEWER);

    // Position at the first entry of the page.
    bool positioned = false;
    if (cursor.isEmpty())
    {
        if (read_forward)
        {
            positioned = LibSystemd::journalSeekHead(journal) >= 0 &&
                         LibSystemd::journalNext(journal) > 0;
            at_oldest_ = true;
        }
        else
        {
            positioned = LibSystemd::journalSeekTail(journal) >= 0 &&
                         LibSystemd::journalPrevious(journal) > 0;
            at_newest_ = true;
        }
    }
    else if (read_forward)
    {
        // Skip to the first entry strictly newer than the cursor.
        if (LibSystemd::journalSeekCursor(journal, cursor.constData()) >= 0 &&
            LibSystemd::journalNext(journal) > 0)
        {
            if (LibSystemd::journalTestCursor(journal, cursor.constData()) > 0)
                positioned = LibSystemd::journalNext(journal) > 0;
            else
                positioned = true;
        }
    }
    else
    {
        // Skip to the first entry strictly older than the cursor.
        if (LibSystemd::journalSeekCursor(journal, cursor.constData()) >= 0 &&
            LibSystemd::journalPrevious(journal) > 0)
        {
            if (LibSystemd::journalTestCursor(journal, cursor.constData()) > 0)
                positioned = LibSystemd::journalPrevious(journal) > 0;
            else
                positioned = true;
        }
    }

    if (positioned)
    {
        QByteArray first_read;
        QByteArray last_read;
        bool more = true;

        for (quint32 i = 0; i < count; ++i)
        {
            const QByteArray current = currentCursor(journal);
            if (i == 0)
                first_read = current;
            last_read = current;

            Record record;

            const QString priority = readField(journal, "PRIORITY");
            record.type = priority.isEmpty() ? Type::INFO : typeFromPriority(priority.toInt());

            uint64_t usec = 0;
            if (LibSystemd::journalGetRealtimeUsec(journal, &usec) >= 0)
                record.time = static_cast<qint64>(usec / 1000000ULL);

            record.source = readField(journal, "SYSLOG_IDENTIFIER");
            if (record.source.isEmpty())
                record.source = readField(journal, "_COMM");

            record.description = readField(journal, "MESSAGE");

            records_.append(std::move(record));

            const int step = read_forward ? LibSystemd::journalNext(journal)
                                          : LibSystemd::journalPrevious(journal);
            if (step <= 0)
            {
                more = false;
                break;
            }
        }

        const bool hit_boundary = !more;

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

    LibSystemd::journalClose(journal);
}

//--------------------------------------------------------------------------------------------------
EventEnumeratorLinux::~EventEnumeratorLinux() = default;

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorLinux::isAtEnd() const
{
    return index_ >= records_.size();
}

//--------------------------------------------------------------------------------------------------
void EventEnumeratorLinux::advance()
{
    ++index_;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type EventEnumeratorLinux::type() const
{
    if (index_ < 0 || index_ >= records_.size())
        return Type::INFO;

    return records_[index_].type;
}

//--------------------------------------------------------------------------------------------------
qint64 EventEnumeratorLinux::time() const
{
    if (index_ < 0 || index_ >= records_.size())
        return 0;

    return records_[index_].time;
}

//--------------------------------------------------------------------------------------------------
quint32 EventEnumeratorLinux::eventId() const
{
    // The journal has no equivalent of the Windows numeric event id.
    return 0;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorLinux::source() const
{
    if (index_ < 0 || index_ >= records_.size())
        return QString();

    return records_[index_].source;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorLinux::description() const
{
    if (index_ < 0 || index_ >= records_.size())
        return QString();

    return records_[index_].description;
}

//--------------------------------------------------------------------------------------------------
QByteArray EventEnumeratorLinux::firstCursor() const
{
    return first_cursor_;
}

//--------------------------------------------------------------------------------------------------
QByteArray EventEnumeratorLinux::lastCursor() const
{
    return last_cursor_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorLinux::atNewest() const
{
    return at_newest_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorLinux::atOldest() const
{
    return at_oldest_;
}
