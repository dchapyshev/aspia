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

#include "base/peer/domain_auth_limiter.h"

#include <QHostAddress>

#include <algorithm>

const DomainAuthLimiter::Policy DomainAuthLimiter::kDefaultAccountPolicy =
{
    3, MilliSeconds(Minutes(1)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1))
};

const DomainAuthLimiter::Policy DomainAuthLimiter::kDefaultAddressPolicy =
{
    5, MilliSeconds(Seconds(30)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1))
};

const MilliSeconds DomainAuthLimiter::kDefaultTrustPeriod = MilliSeconds(Hours(24 * 30));

//--------------------------------------------------------------------------------------------------
DomainAuthLimiter::DomainAuthLimiter()
    : DomainAuthLimiter(kDefaultAccountPolicy, kDefaultAddressPolicy, kDefaultMaxEntries)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DomainAuthLimiter::DomainAuthLimiter(const Policy& account_policy, const Policy& address_policy,
                                     int max_entries, int max_in_flight_per_address,
                                     MilliSeconds trust_period)
    : account_policy_(account_policy),
      address_policy_(address_policy),
      max_entries_(std::max(max_entries, 1)),
      max_in_flight_per_address_(std::max(max_in_flight_per_address, 1)),
      trust_period_(trust_period)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
QString DomainAuthLimiter::accountKey(const QString& account)
{
    // NFKC first: compatibility forms (full-width letters, ligatures) become the plain characters
    // that the directory compares them to, and "\" / "@" written as full-width forms are found.
    QString name = account.normalized(QString::NormalizationForm_KC).trimmed();

    const qsizetype backslash = name.lastIndexOf(QLatin1Char('\\'));
    if (backslash != -1)
        name = name.mid(backslash + 1);

    const qsizetype at = name.indexOf(QLatin1Char('@'));
    if (at > 0)
        name = name.left(at);

    return name.trimmed().toCaseFolded();
}

//--------------------------------------------------------------------------------------------------
// static
QString DomainAuthLimiter::addressKey(const QString& address)
{
    QString text = address.trimmed();

    // "[::1]" and a zone ("fe80::1%eth0") are not part of the network.
    if (text.startsWith(QLatin1Char('[')) && text.endsWith(QLatin1Char(']')))
        text = text.mid(1, text.size() - 2);
    const qsizetype zone = text.indexOf(QLatin1Char('%'));
    if (zone != -1)
        text = text.left(zone);

    QHostAddress host_address;
    if (!host_address.setAddress(text))
        return address.trimmed().toLower();

    bool is_ipv4 = false;
    const quint32 ipv4 = host_address.toIPv4Address(&is_ipv4);
    if (is_ipv4)
        return QHostAddress(ipv4).toString();

    Q_IPV6ADDR ipv6 = host_address.toIPv6Address();
    for (int i = 8; i < 16; ++i)
        ipv6[i] = 0;

    return QHostAddress(ipv6).toString() + QLatin1String("/64");
}

//--------------------------------------------------------------------------------------------------
DomainAuthLimiter::Decision DomainAuthLimiter::acquire(
    const QString& account, const QString& address, TimePoint now, Ticket* ticket,
    MilliSeconds* blocked_for)
{
    if (blocked_for)
        *blocked_for = MilliSeconds::zero();
    if (!ticket)
        return Decision::BUSY;

    *ticket = Ticket();

    const QString account_key = accountKey(account);
    const QString address_key = addressKey(address);

    QMutexLocker locker(&lock_);

    const MilliSeconds blocked = blockedForLocked(account_key, address_key, now);
    if (blocked > MilliSeconds::zero())
    {
        if (blocked_for)
            *blocked_for = blocked;
        return Decision::BLOCKED;
    }

    auto account_it = accounts_.map.find(account_key);
    if (account_it != accounts_.map.end() && account_it->pending >= 1)
        return Decision::BUSY;

    auto address_it = addresses_.map.find(address_key);
    if (address_it != addresses_.map.end() && address_it->pending >= max_in_flight_per_address_)
        return Decision::BUSY;

    Entry* account_entry = findOrCreate(&accounts_, account_policy_, account_key, now);
    if (!account_entry)
        return Decision::BUSY;

    Entry* address_entry = findOrCreate(&addresses_, address_policy_, address_key, now);
    if (!address_entry)
    {
        dropIfEmpty(&accounts_, account_key);
        return Decision::BUSY;
    }

    unindex(&accounts_, account_entry);
    unindex(&addresses_, address_entry);
    ++account_entry->pending;
    ++address_entry->pending;

    ticket->account_key = account_key;
    ticket->address_key = address_key;
    ticket->valid = true;
    return Decision::ALLOWED;
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::release(Ticket* ticket, Outcome outcome, TimePoint now,
                                const QString& canonical_account)
{
    if (!ticket || !ticket->valid)
        return;

    QMutexLocker locker(&lock_);

    auto finish = [&](Table* table, const Policy& policy, const QString& key)
    {
        auto it = table->map.find(key);
        if (it == table->map.end())
            return;

        it->pending = std::max(it->pending - 1, 0);

        if (outcome == Outcome::FAILURE)
            addFailureLocked(table, policy, key, now);
        else if (outcome == Outcome::SUCCESS)
            resetLocked(table, key);

        it = table->map.find(key);
        if (it == table->map.end())
            return;
        if (it->pending == 0)
            reindex(table, policy, key, &*it);
        dropIfEmpty(table, key);
    };

    finish(&accounts_, account_policy_, ticket->account_key);
    finish(&addresses_, address_policy_, ticket->address_key);

    if (outcome == Outcome::SUCCESS)
    {
        trustLocked(ticket->account_key, ticket->address_key, now);

        if (!canonical_account.isEmpty())
        {
            const QString canonical_key = accountKey(canonical_account);
            if (canonical_key != ticket->account_key)
            {
                resetLocked(&accounts_, canonical_key);
                trustLocked(canonical_key, ticket->address_key, now);
            }
        }
    }

    *ticket = Ticket();
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::addAddressFailure(const QString& address, TimePoint now)
{
    QMutexLocker locker(&lock_);
    addFailureLocked(&addresses_, address_policy_, addressKey(address), now);
}

//--------------------------------------------------------------------------------------------------
MilliSeconds DomainAuthLimiter::blockedFor(
    const QString& account, const QString& address, TimePoint now) const
{
    const QString account_key = accountKey(account);
    const QString address_key = addressKey(address);

    QMutexLocker locker(&lock_);
    return blockedForLocked(account_key, address_key, now);
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::addFailure(const QString& account, const QString& address, TimePoint now)
{
    const QString account_key = accountKey(account);
    const QString address_key = addressKey(address);

    QMutexLocker locker(&lock_);
    addFailureLocked(&accounts_, account_policy_, account_key, now);
    addFailureLocked(&addresses_, address_policy_, address_key, now);
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::addSuccess(const QString& account, const QString& address, TimePoint now)
{
    const QString account_key = accountKey(account);
    const QString address_key = addressKey(address);

    QMutexLocker locker(&lock_);
    resetLocked(&accounts_, account_key);
    resetLocked(&addresses_, address_key);
    trustLocked(account_key, address_key, now);
}

//--------------------------------------------------------------------------------------------------
int DomainAuthLimiter::accountCount() const
{
    QMutexLocker locker(&lock_);
    return static_cast<int>(accounts_.map.size());
}

//--------------------------------------------------------------------------------------------------
int DomainAuthLimiter::addressCount() const
{
    QMutexLocker locker(&lock_);
    return static_cast<int>(addresses_.map.size());
}

//--------------------------------------------------------------------------------------------------
// static
MilliSeconds DomainAuthLimiter::remaining(const Table& table, const QString& key, TimePoint now)
{
    auto it = table.map.constFind(key);
    if (it == table.map.constEnd() || it->blocked_until <= now)
        return MilliSeconds::zero();

    // Round up, so that a remaining fraction of a millisecond is still "blocked".
    return std::chrono::ceil<MilliSeconds>(it->blocked_until - now);
}

//--------------------------------------------------------------------------------------------------
bool DomainAuthLimiter::isTrusted(
    const QString& account_key, const QString& address_key, TimePoint now) const
{
    auto it = trusted_.constFind(account_key);
    if (it == trusted_.constEnd())
        return false;

    for (const Trust& trust : *it)
    {
        if (trust.address_key == address_key && trust.until > now)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
MilliSeconds DomainAuthLimiter::blockedForLocked(
    const QString& account_key, const QString& address_key, TimePoint now) const
{
    const MilliSeconds address_block = remaining(addresses_, address_key, now);

    // A source that recently signed in to this account is not affected by its lockout.
    const MilliSeconds account_block = isTrusted(account_key, address_key, now) ?
        MilliSeconds::zero() : remaining(accounts_, account_key, now);

    return std::max(address_block, account_block);
}

//--------------------------------------------------------------------------------------------------
DomainAuthLimiter::Entry* DomainAuthLimiter::findOrCreate(
    Table* table, const Policy& /* policy */, const QString& key, TimePoint now)
{
    auto it = table->map.find(key);
    if (it != table->map.end())
        return &*it;

    if (table->map.size() >= max_entries_)
    {
        // Drop the entry that became droppable longest ago, if it is droppable already. Entries
        // with a logon in progress are not in the index; blocked or remembered ones sort later.
        auto oldest = table->index.begin();
        if (oldest == table->index.end() || oldest->first > now)
            return nullptr; // Full of active entries: refuse the new key.

        const QString victim = oldest->second;
        table->index.erase(oldest);
        table->map.remove(victim);
    }

    return &*table->map.insert(key, Entry());
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::unindex(Table* table, Entry* entry)
{
    if (!entry->indexed)
        return;

    table->index.erase(entry->index_it);
    entry->indexed = false;
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::reindex(Table* table, const Policy& policy, const QString& key, Entry* entry)
{
    unindex(table, entry);

    if (entry->pending > 0)
        return;

    // Droppable once the block is over and the failures are forgotten. Under pressure the
    // remembered-but-not-blocked entries go first (they sort by block end, not by forget time).
    TimePoint droppable_from = entry->blocked_until;
    if (entry->failures > 0 && entry->blocked_until <= entry->last_failure)
        droppable_from = entry->last_failure;
    Q_UNUSED(policy);

    entry->index_it = table->index.emplace(droppable_from, key);
    entry->indexed = true;
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::dropIfEmpty(Table* table, const QString& key)
{
    auto it = table->map.find(key);
    if (it == table->map.end())
        return;

    if (it->failures == 0 && it->pending == 0)
    {
        unindex(table, &*it);
        table->map.erase(it);
    }
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::addFailureLocked(Table* table, const Policy& policy, const QString& key,
                                         TimePoint now)
{
    Entry* entry = findOrCreate(table, policy, key, now);
    if (!entry)
        return; // Full of active entries; the source / account limits of others still apply.

    // A key that has not failed for a long time starts from scratch.
    if (entry->failures > 0 && now - entry->last_failure >= policy.forget_after)
    {
        entry->failures = 0;
        entry->blocked_until = TimePoint();
    }

    entry->failures = std::min(entry->failures + 1, 1000);
    entry->last_failure = now;

    // The |free_failures|-th failure starts the backoff.
    const int over = entry->failures - policy.free_failures + 1;
    if (over > 0)
    {
        // base * 2^(over - 1), saturated at max_delay without overflowing.
        MilliSeconds delay = policy.base_delay;
        for (int i = 1; i < over && delay < policy.max_delay; ++i)
            delay *= 2;
        delay = std::min(delay, policy.max_delay);

        entry->blocked_until = std::max(entry->blocked_until, now + delay);
    }

    if (entry->pending == 0)
        reindex(table, policy, key, entry);
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::resetLocked(Table* table, const QString& key)
{
    auto it = table->map.find(key);
    if (it == table->map.end())
        return;

    if (it->pending > 0)
    {
        it->failures = 0;
        it->blocked_until = TimePoint();
        return;
    }

    unindex(table, &*it);
    table->map.erase(it);
}

//--------------------------------------------------------------------------------------------------
void DomainAuthLimiter::trustLocked(const QString& account_key, const QString& address_key,
                                    TimePoint now)
{
    if (trust_period_ <= MilliSeconds::zero() || account_key.isEmpty() || address_key.isEmpty())
        return;

    // Bounded like the other tables: a full table forgets expired trusts first, then refuses.
    if (!trusted_.contains(account_key) && trusted_.size() >= max_entries_)
    {
        for (auto it = trusted_.begin(); it != trusted_.end();)
        {
            std::erase_if(*it, [now](const Trust& t) { return t.until <= now; });
            it = it->empty() ? trusted_.erase(it) : std::next(it);
        }
        if (trusted_.size() >= max_entries_)
            return;
    }

    std::vector<Trust>& list = trusted_[account_key];
    std::erase_if(list, [&](const Trust& t) { return t.until <= now || t.address_key == address_key; });

    if (list.size() >= static_cast<size_t>(kMaxTrustedAddressesPerAccount))
    {
        auto oldest = std::min_element(list.begin(), list.end(),
            [](const Trust& a, const Trust& b) { return a.until < b.until; });
        list.erase(oldest);
    }

    list.push_back({ address_key, now + trust_period_ });
}
