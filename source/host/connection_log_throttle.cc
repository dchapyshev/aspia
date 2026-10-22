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

#include "host/connection_log_throttle.h"

//--------------------------------------------------------------------------------------------------
ConnectionLogThrottle::ConnectionLogThrottle(MilliSeconds interval)
    : interval_(interval)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool ConnectionLogThrottle::shouldLog(const QString& address, TimePoint now, int* suppressed)
{
    *suppressed = 0;

    auto it = entries_.find(address);
    if (it != entries_.end())
    {
        if (now - it->last_logged < interval_)
        {
            ++it->suppressed;
            return false;
        }

        *suppressed = it->suppressed;
        it->suppressed = 0;
        it->last_logged = now;
        return true;
    }

    // Memory is bounded: drop the entries whose interval has passed; if all are recent (a flood
    // from many sources), start over. Logging more is the safe direction.
    if (entries_.size() >= kMaxSources)
    {
        for (auto entry = entries_.begin(); entry != entries_.end();)
        {
            if (now - entry->last_logged >= interval_)
                entry = entries_.erase(entry);
            else
                ++entry;
        }

        if (entries_.size() >= kMaxSources)
            entries_.clear();
    }

    Entry entry;
    entry.last_logged = now;
    entries_.insert(address, entry);
    return true;
}
