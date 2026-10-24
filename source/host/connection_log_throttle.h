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

#ifndef HOST_CONNECTION_LOG_THROTTLE_H
#define HOST_CONNECTION_LOG_THROTTLE_H

#include <QHash>
#include <QString>

#include "base/time_types.h"

// Rate limit for security log lines of connections that end before any credentials are sent
// (online status probes of clients, port scans). The first one from a source is logged, the
// following ones within |interval| are only counted and reported with the next logged line.
// Failed logons are never passed through this class. Not thread-safe (used on the service thread).
class ConnectionLogThrottle
{
public:
    static constexpr int kMaxSources = 4096;

    explicit ConnectionLogThrottle(MilliSeconds interval = MilliSeconds(Minutes(10)));

    // True if the event from |address| is to be logged now; |suppressed| is the number of events
    // from it that were not logged since the last logged one.
    bool shouldLog(const QString& address, TimePoint now, int* suppressed);

private:
    struct Entry
    {
        TimePoint last_logged;
        int suppressed = 0;
    };

    const MilliSeconds interval_;
    QHash<QString, Entry> entries_;
};

#endif // HOST_CONNECTION_LOG_THROTTLE_H
