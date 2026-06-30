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

#include <QByteArray>

#include "base/logging.h"
#include "base/linux/libsystemd.h"

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

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumeratorLinux::EventEnumeratorLinux(const QString& log_name, quint32 start, quint32 count)
{
    if (!count)
        return;

    int ret = LibSystemd::journalOpen(&journal_);
    if (ret < 0)
    {
        LOG(ERROR) << "journalOpen failed:" << ret;
        journal_ = nullptr;
        return;
    }

    addLogMatches(journal_, log_name);

    // Count the total number of matching entries for the client's pagination.
    if (LibSystemd::journalSeekHead(journal_) >= 0)
    {
        while (LibSystemd::journalNext(journal_) > 0)
            ++records_count_;
    }

    // Position at the newest entry, then skip |start| entries going backwards.
    if (LibSystemd::journalSeekTail(journal_) < 0 || LibSystemd::journalPrevious(journal_) <= 0)
    {
        at_end_ = true;
        return;
    }

    for (quint32 i = 0; i < start; ++i)
    {
        if (LibSystemd::journalPrevious(journal_) <= 0)
        {
            at_end_ = true;
            return;
        }
    }

    remaining_ = static_cast<int>(count);
}

//--------------------------------------------------------------------------------------------------
EventEnumeratorLinux::~EventEnumeratorLinux()
{
    if (journal_)
        LibSystemd::journalClose(journal_);
}

//--------------------------------------------------------------------------------------------------
quint32 EventEnumeratorLinux::count() const
{
    return records_count_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorLinux::isAtEnd() const
{
    return at_end_ || !journal_ || remaining_ <= 0;
}

//--------------------------------------------------------------------------------------------------
void EventEnumeratorLinux::advance()
{
    if (!journal_)
    {
        at_end_ = true;
        return;
    }

    --remaining_;
    if (remaining_ <= 0 || LibSystemd::journalPrevious(journal_) <= 0)
        at_end_ = true;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type EventEnumeratorLinux::type() const
{
    const QString priority = readField("PRIORITY");
    if (priority.isEmpty())
        return Type::INFO;

    return typeFromPriority(priority.toInt());
}

//--------------------------------------------------------------------------------------------------
qint64 EventEnumeratorLinux::time() const
{
    if (!journal_)
        return 0;

    uint64_t usec = 0;
    if (LibSystemd::journalGetRealtimeUsec(journal_, &usec) < 0)
        return 0;

    return static_cast<qint64>(usec / 1000000ULL);
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorLinux::category() const
{
    // The journal has no equivalent of the Windows task category.
    return QString();
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
    QString identifier = readField("SYSLOG_IDENTIFIER");
    if (identifier.isEmpty())
        identifier = readField("_COMM");

    return identifier;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorLinux::description() const
{
    return readField("MESSAGE");
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorLinux::readField(const char* field) const
{
    if (!journal_)
        return QString();

    const void* data = nullptr;
    size_t length = 0;
    if (LibSystemd::journalGetData(journal_, field, &data, &length) < 0)
        return QString();

    // The returned data has the form "FIELD=value"; strip the "FIELD=" prefix.
    const QByteArray raw(static_cast<const char*>(data), static_cast<qsizetype>(length));
    int separator = raw.indexOf('=');
    if (separator < 0)
        return QString();

    return QString::fromUtf8(raw.mid(separator + 1));
}
