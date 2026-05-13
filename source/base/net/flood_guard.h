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
#include <asio/ip/tcp.hpp>

#include <chrono>
#include <unordered_map>

// Two-pronged anti-flood gate for an accept loop. Exposes a single public entry point check()
// that runs both gates against the same time sample:
//
//   1) Per-address rate limit - bounds how often a single source can attempt to connect over a
//      sliding window, smoothing bursts. Tunable at runtime via setRateLimit().
//   2) Global pending cap - bounds how many half-finished handshakes may be in flight at once,
//      regardless of source. Tunable at runtime via setMaxPending().
//
// check() is advisory: returns a bool the caller uses to drop the connection. Each rejected
// gate emits a LOG(WARNING) rate-limited to at most one entry per kMinLogInterval, so an
// attacker can't flood the log by hammering the accept loop.
//
// Per-address algorithm: GCRA (Generic Cell Rate Algorithm, ITU-T I.371), the integer-math
// equivalent of a token bucket. Per source we store one TimePoint - the Theoretical Arrival
// Time. Each allowed call advances TAT by |slot_| (= window / max_per_window); a call is
// rejected when the new TAT would lie more than |burst_| (= window) ahead of now. The bucket
// state is implicit: |now - TAT| corresponds to accumulated tokens, |TAT - now| to spent
// burst credit. Compared to an explicit token bucket this halves per-entry memory and avoids
// floating point. The map is capped at |kMaxTracked| entries; when full, fully-refilled
// entries (TAT <= now) are evicted first. If none can be freed the guard fails open for new
// addresses - that prevents a botnet-scale flood from evicting legitimate peers as collateral.
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

    // Constructs with conservative defaults: 60 connections/min per address, pending cap 32.
    // Callers tune the limits to their role via setRateLimit() and setMaxPending() before the
    // accept loop starts.
    FloodGuard() = default;

    // Single entry point for an accept loop. Runs the per-address rate check on |socket|'s
    // peer and the global pending-cap check against |current_pending|, both against the same
    // |now| sampled internally. Returns true only if both gates allow the connection. Each
    // rejected gate emits a rate-limited LOG(WARNING) summarising suppressed rejections.
    //
    // Per-address check runs first so a flooding source is attributed to the per-IP log stream
    // rather than masked by the global "pending limit reached" warning.
    bool check(const asio::ip::tcp::socket& socket, int current_pending);

    // Updates the per-address sliding-window rate. |window| is the smoothing horizon; over it
    // each address is allowed |max_per_window| connections, with bursts of the same size
    // absorbed before throttling. Intended to be called before the accept loop starts. Existing
    // per-address TAT entries are kept; the new |slot_|/|burst_| apply to subsequent calls.
    void setRateLimit(Seconds window, int max_per_window);

    // Updates / reads the global pending cap. Intended to be called before the accept loop
    // starts so the value is stable for the lifetime of the listener.
    void setMaxPending(int value);
    int maxPending() const;

private:
    // Test-only access to the underlying per-address and per-pending checks.
    friend class FloodGuardTestPeer;

    bool checkAddress(TimePoint now, const asio::ip::address& address);
    bool checkAddress(TimePoint now, const asio::ip::tcp::socket& socket);
    bool checkPending(TimePoint now, int current_pending);

    struct AddressHash
    {
        size_t operator()(const asio::ip::address& address) const noexcept;
    };

    // Hard cap on the per-address state map. Chosen so that a botnet-scale source diversity
    // can't blow up memory; ~10K entries of (address + TimePoint) sits well under 1 MB.
    static constexpr int kMaxTracked = 10000;

    int max_pending_ = 32;
    Nanoseconds slot_ = std::chrono::seconds(1);   // Cost per accept (= window / max_per_window).
    Nanoseconds burst_ = std::chrono::seconds(60); // Burst tolerance (= window).

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
