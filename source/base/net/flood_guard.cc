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

#include "base/net/flood_guard.h"

#include <bit>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
size_t FloodGuard::AddressHash::operator()(const asio::ip::address& address) const noexcept
{
    if (address.is_v4())
        return std::hash<quint32>{}(address.to_v4().to_uint());

    const auto halves = std::bit_cast<std::array<quint64, 2>>(address.to_v6().to_bytes());
    return std::hash<quint64>{}(halves[0] ^ halves[1]);
}

//--------------------------------------------------------------------------------------------------
bool FloodGuard::check(const asio::ip::tcp::socket& socket, int current_pending)
{
    const TimePoint now = Clock::now();
    return checkAddress(now, socket) && checkPending(now, current_pending);
}

//--------------------------------------------------------------------------------------------------
void FloodGuard::setRateLimit(Seconds window, int max_per_window)
{
    CHECK_GT(window.count(), 0);
    CHECK_GT(max_per_window, 0);

    slot_ = Nanoseconds(window) / max_per_window;
    burst_ = window;
}

//--------------------------------------------------------------------------------------------------
void FloodGuard::setMaxPending(int value)
{
    max_pending_ = value;
    CHECK_GT(max_pending_, 0);
}

//--------------------------------------------------------------------------------------------------
int FloodGuard::maxPending() const
{
    return max_pending_;
}

//--------------------------------------------------------------------------------------------------
bool FloodGuard::checkAddress(TimePoint now, const asio::ip::tcp::socket& socket)
{
    std::error_code error_code;
    const asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(error_code);
    if (error_code)
        return true;
    return checkAddress(now, endpoint.address());
}

//--------------------------------------------------------------------------------------------------
bool FloodGuard::checkAddress(TimePoint now, const asio::ip::address& address)
{
    auto it = tat_.find(address);
    if (it == tat_.end())
    {
        if (static_cast<int>(tat_.size()) >= kMaxTracked)
        {
            // Map is full. Drop entries whose TAT lies in the past (fully refilled buckets);
            // removing them does not affect rate-limiting accuracy - on their next attempt
            // they would be treated as fresh anyway.
            for (auto sweep_it = tat_.begin(); sweep_it != tat_.end(); )
            {
                if (sweep_it->second <= now)
                    sweep_it = tat_.erase(sweep_it);
                else
                    ++sweep_it;
            }

            // Still full - the guard is overwhelmed by distinct active sources. Fail open:
            // track nothing new, allow the connection. This prevents a botnet-scale flood from
            // implicitly DoSing legitimate peers (they would get evicted on every accept).
            if (static_cast<int>(tat_.size()) >= kMaxTracked)
                return true;
        }

        tat_.emplace(address, now + slot_);
        return true;
    }

    // GCRA: spending one slot advances TAT by |slot_|. If the new TAT lies more than |burst_|
    // ahead of |now|, the source has exceeded its burst budget - reject.
    const TimePoint tat = std::max(it->second, now);
    const TimePoint new_tat = tat + slot_;

    if (new_tat - now > burst_)
    {
        ++rejected_address_since_last_warning_;

        constexpr Seconds kMinLogInterval{ 30 };

        if (last_address_warning_ == TimePoint() || (now - last_address_warning_) >= kMinLogInterval)
        {
            LOG(WARNING) << "Per-address rate limit hit for" << address.to_string()
                         << "; rejected " << rejected_address_since_last_warning_
                         << " connection(s) since last warning";
            rejected_address_since_last_warning_ = 0;
            last_address_warning_ = now;
        }
        return false;
    }

    it->second = new_tat;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool FloodGuard::checkPending(TimePoint now, int current_pending)
{
    if (current_pending < max_pending_)
        return true;

    ++rejected_pending_since_last_warning_;

    constexpr Seconds kMinLogInterval{ 30 };

    if (last_pending_warning_ == TimePoint() || (now - last_pending_warning_) >= kMinLogInterval)
    {
        LOG(WARNING) << "Pending connection limit reached (" << current_pending
                     << "); rejected " << rejected_pending_since_last_warning_
                     << " connection(s) since last warning";
        rejected_pending_since_last_warning_ = 0;
        last_pending_warning_ = now;
    }

    return false;
}
