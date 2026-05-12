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

#ifndef BASE_NET_FLOOD_GUARD_H
#define BASE_NET_FLOOD_GUARD_H

#include <asio/ip/address.hpp>

#include <chrono>
#include <unordered_map>

// Two-pronged anti-flood gate for an accept loop:
//
//   1) Per-address rate limit (checkAddress) - bounds how often a single source can attempt to
//      connect over a sliding window, smoothing bursts.
//   2) Global pending cap (checkPending) - bounds how many half-finished handshakes may be
//      in flight at once, regardless of source.
//
// Both methods are advisory: they return a bool that the caller uses to drop the connection.
// On rejection each method emits a LOG(WARNING) rate-limited to at most one entry per
// kMinLogInterval, so an attacker can't flood the log by hammering the accept loop.
//
// Per-address algorithm: GCRA (Generic Cell Rate Algorithm, ITU-T I.371), the integer-math
// equivalent of a token bucket. Per source we store one TimePoint - the Theoretical Arrival
// Time. Each allowed call advances TAT by |slot_| (= window / max_per_window); a call is
// rejected when the new TAT would lie more than |burst_| (= window) ahead of now. The bucket
// state is implicit: |now - TAT| corresponds to accumulated tokens, |TAT - now| to spent
// burst credit. Compared to an explicit token bucket this halves per-entry memory and avoids
// floating point. The map is capped at |max_tracked| entries; when full, fully-refilled
// entries (TAT <= now) are evicted first. If none can be freed the guard fails open for new
// addresses - that prevents a botnet-scale flood from churning out legitimate peers as
// collateral.
//
// Pending-cap algorithm: trivial threshold against the caller-supplied current count.
//
// Not thread-safe. Intended to be called only from the asio io_context thread that owns the
// accept loop.
class FloodGuard
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Seconds = std::chrono::seconds;
    using Nanoseconds = std::chrono::nanoseconds;

    // |window|         - sliding window for the per-address rate (seconds).
    // |max_per_window| - max connections accepted from one address per |window|, with bursts
    //                    of up to |max_per_window| absorbed before throttling kicks in.
    // |max_pending|    - global cap on in-flight handshakes; checkPending() enforces it.
    // |max_tracked|    - cap on the per-address state map. When full, fully-refilled entries
    //                    are evicted first; if none can be freed the guard fails open for new
    //                    addresses so a botnet-scale flood does not starve legitimate peers.
    FloodGuard(Seconds window, int max_per_window, int max_pending, int max_tracked = 10000);

    // |now| is taken as a parameter rather than queried inside the methods so the caller can
    // fetch Clock::now() once and pass it to both checks for the same accept event.

    // Returns true if a connection from |address| is currently within its per-address budget.
    // On rejection emits a rate-limited LOG(WARNING) summarising suppressed rejections.
    bool checkAddress(TimePoint now, const asio::ip::address& address);

    // Returns true while |current_pending| is below the cap configured in the constructor.
    // Once the cap is hit, returns false and emits a rate-limited LOG(WARNING) summarising
    // suppressed rejections. Independent of checkAddress() - tracks its own rejection counter
    // so log streams don't get mixed.
    bool checkPending(TimePoint now, int current_pending);

private:
    struct AddressHash
    {
        size_t operator()(const asio::ip::address& address) const noexcept;
    };

    const int max_pending_;
    const int max_tracked_;
    const Nanoseconds slot_;  // Cost per accept (= window / max_per_window).
    const Nanoseconds burst_; // Burst tolerance (= window).

    // Per-address theoretical arrival time. Distance from |now| measures how far ahead of the
    // steady rate the source has run; |now - tat| measures how much idle headroom remains.
    std::unordered_map<asio::ip::address, TimePoint, AddressHash> tat_;

    // Rate-limited logging state. Separate counters for the per-address rejection path and
    // the pending-limit path so the two log streams report meaningful counts. We emit at most
    // one warning per kMinLogInterval and accumulate suppressed rejections in between.
    // Default-constructed time_point is used as the "never logged yet" sentinel.
    TimePoint last_address_warning_;
    int rejected_address_since_last_warning_ = 0;

    TimePoint last_pending_warning_;
    int rejected_pending_since_last_warning_ = 0;
};

#endif // BASE_NET_FLOOD_GUARD_H
