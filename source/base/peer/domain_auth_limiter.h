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

#ifndef BASE_PEER_DOMAIN_AUTH_LIMITER_H
#define BASE_PEER_DOMAIN_AUTH_LIMITER_H

#include <QHash>
#include <QMutex>
#include <QString>

#include <map>
#include <utility>
#include <vector>

#include "base/time_types.h"

// Slows down online password guessing for operating system / domain accounts (IDENTIFY_PASSWORD).
//
// Failed logons are counted per account and per source network (an IPv4 address, or the /64 prefix
// of an IPv6 address, so that rotating addresses inside one subnet does not help). After
// |free_failures| failures every further failure blocks the key for an exponentially growing time
// (|base_delay| * 2^n, at most |max_delay|). A blocked key is refused without asking the operating
// system. There is no permanent lock: the counter is forgotten |forget_after| after the last
// failure, and a successful logon resets the account and the source.
//
// Check and reservation are one atomic step (acquire()): while a logon of an account is being
// checked by the operating system, no second logon of the same account is started, and a source
// has at most |max_in_flight_per_address| logons in progress. Without this, parallel connections
// would all pass the check before the first failure is recorded (and could lock the account in
// the domain within a second).
//
// Trusted sources: a source that has signed in successfully to an account within |trust_period|
// is not blocked by the lockout of that account (an attacker elsewhere can not lock its owner
// out). It is still subject to the limits of the source itself and to the single-logon rule.
//
// Attempts refused while blocked do not extend the block. Memory is bounded: when a table is full
// the entry whose block has expired longest ago is dropped (O(log n)); entries that are blocked or
// have a logon in progress are never dropped, a new key is refused instead.
//
// Thread-safe.
class DomainAuthLimiter
{
public:
    struct Policy
    {
        int free_failures;
        MilliSeconds base_delay;
        MilliSeconds max_delay;
        MilliSeconds forget_after;
    };

    static const Policy kDefaultAccountPolicy;
    static const Policy kDefaultAddressPolicy;
    static constexpr int kDefaultMaxEntries = 10000;
    static constexpr int kDefaultMaxInFlightPerAddress = 2;
    static constexpr int kMaxTrustedAddressesPerAccount = 8;
    static const MilliSeconds kDefaultTrustPeriod;

    enum class Decision
    {
        ALLOWED, // Reserved; release() must be called.
        BLOCKED, // The account or the source is in backoff.
        BUSY     // A logon of this account (or too many from this source) is already in progress,
                 // or the tables are full of active entries.
    };

    enum class Outcome { SUCCESS, FAILURE, CANCELLED };

    // A reservation made by acquire().
    struct Ticket
    {
        QString account_key;
        QString address_key;
        bool valid = false;
    };

    DomainAuthLimiter();
    DomainAuthLimiter(const Policy& account_policy, const Policy& address_policy, int max_entries,
                      int max_in_flight_per_address = kDefaultMaxInFlightPerAddress,
                      MilliSeconds trust_period = kDefaultTrustPeriod);
    ~DomainAuthLimiter() = default;

    // Normalized key of an account: the name without the domain ("CORP\Ivanov",
    // "ivanov@corp.example.com" and "ivanov" are the same key), Unicode NFKC, case folded.
    static QString accountKey(const QString& account);

    // Normalized key of a source address: IPv4 as is (also an IPv4-mapped IPv6), IPv6 cut to the /64
    // prefix. An address that can not be parsed is used as is.
    static QString addressKey(const QString& address);

    // Atomically checks the limits and reserves a logon of |account| from |address|.
    Decision acquire(const QString& account, const QString& address, TimePoint now, Ticket* ticket,
                     MilliSeconds* blocked_for = nullptr);

    // Ends a reservation. CANCELLED (the operating system was not asked) changes no counter.
    // |canonical_account| (optional, from the operating system) is also reset on success.
    void release(Ticket* ticket, Outcome outcome, TimePoint now,
                 const QString& canonical_account = QString());

    // Records a malformed request from |address| (counts against the source only).
    void addAddressFailure(const QString& address, TimePoint now);

    // How long logons of |account| from |address| are still refused. Zero if allowed.
    MilliSeconds blockedFor(const QString& account, const QString& address, TimePoint now) const;

    // Unreserved variants (tests and callers that do not need the reservation).
    void addFailure(const QString& account, const QString& address, TimePoint now);
    void addSuccess(const QString& account, const QString& address, TimePoint now = Clock::now());

    int accountCount() const;
    int addressCount() const;

private:
    struct Entry
    {
        int failures = 0;
        int pending = 0;
        TimePoint last_failure;
        TimePoint blocked_until;
        // Position in the eviction index (valid only while pending == 0).
        bool indexed = false;
        std::multimap<TimePoint, QString>::iterator index_it;
    };

    struct Table
    {
        QHash<QString, Entry> map;
        // Ordered by the time from which the entry may be dropped (max(blocked_until,
        // last_failure + forget_after) for remembered failures, or the block end).
        std::multimap<TimePoint, QString> index;
    };

    struct Trust
    {
        QString address_key;
        TimePoint until;
    };

    static MilliSeconds remaining(const Table& table, const QString& key, TimePoint now);
    bool isTrusted(const QString& account_key, const QString& address_key, TimePoint now) const;
    MilliSeconds blockedForLocked(const QString& account_key, const QString& address_key,
                                  TimePoint now) const;
    Entry* findOrCreate(Table* table, const Policy& policy, const QString& key, TimePoint now);
    void unindex(Table* table, Entry* entry);
    void reindex(Table* table, const Policy& policy, const QString& key, Entry* entry);
    void dropIfEmpty(Table* table, const QString& key);
    void addFailureLocked(Table* table, const Policy& policy, const QString& key, TimePoint now);
    void resetLocked(Table* table, const QString& key);
    void trustLocked(const QString& account_key, const QString& address_key, TimePoint now);

    const Policy account_policy_;
    const Policy address_policy_;
    const int max_entries_;
    const int max_in_flight_per_address_;
    const MilliSeconds trust_period_;

    mutable QMutex lock_;
    Table accounts_;
    Table addresses_;
    QHash<QString, std::vector<Trust>> trusted_;

    Q_DISABLE_COPY_MOVE(DomainAuthLimiter)
};

#endif // BASE_PEER_DOMAIN_AUTH_LIMITER_H
