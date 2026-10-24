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

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

const DomainAuthLimiter::Policy kAccountPolicy =
{
    3, MilliSeconds(Seconds(60)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1))
};

const DomainAuthLimiter::Policy kAddressPolicy =
{
    5, MilliSeconds(Seconds(30)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1))
};

const QString kAddress = QStringLiteral("192.0.2.10");

} // namespace

TEST(domain_auth_limiter_test, account_key)
{
    EXPECT_EQ(DomainAuthLimiter::accountKey("CORP\\Ivanov"), "ivanov");
    EXPECT_EQ(DomainAuthLimiter::accountKey("ivanov@corp.example.com"), "ivanov");
    EXPECT_EQ(DomainAuthLimiter::accountKey(" IVANOV "), "ivanov");
    EXPECT_EQ(DomainAuthLimiter::accountKey("OTHER\\ivanov"), DomainAuthLimiter::accountKey("ivanov"));
    EXPECT_NE(DomainAuthLimiter::accountKey("petrov"), DomainAuthLimiter::accountKey("ivanov"));
}

TEST(domain_auth_limiter_test, address_key)
{
    EXPECT_EQ(DomainAuthLimiter::addressKey("192.0.2.10"), "192.0.2.10");
    EXPECT_EQ(DomainAuthLimiter::addressKey("::ffff:192.0.2.10"), "192.0.2.10");
    EXPECT_NE(DomainAuthLimiter::addressKey("192.0.2.10"), DomainAuthLimiter::addressKey("192.0.2.11"));

    // The whole IPv6 /64 is one source.
    EXPECT_EQ(DomainAuthLimiter::addressKey("2001:db8:1:2:aaaa::1"),
              DomainAuthLimiter::addressKey("2001:db8:1:2:ffff:ffff:ffff:ffff"));
    EXPECT_EQ(DomainAuthLimiter::addressKey("[2001:db8:1:2::5]"),
              DomainAuthLimiter::addressKey("2001:db8:1:2::6"));
    EXPECT_EQ(DomainAuthLimiter::addressKey("fe80::1%eth0"), DomainAuthLimiter::addressKey("fe80::2"));
    EXPECT_NE(DomainAuthLimiter::addressKey("2001:db8:1:2::1"),
              DomainAuthLimiter::addressKey("2001:db8:1:3::1"));
}

TEST(domain_auth_limiter_test, free_failures_then_exponential_backoff)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    for (int i = 0; i < 2; ++i)
    {
        limiter.addFailure("CORP\\ivanov", kAddress + QString::number(i), now);
        EXPECT_EQ(limiter.blockedFor("CORP\\ivanov", "198.51.100.1", now), MilliSeconds::zero());
    }

    // The third failure starts the backoff: 60 s.
    limiter.addFailure("ivanov", "198.51.100.2", now);
    EXPECT_EQ(limiter.blockedFor("ivanov@corp", "198.51.100.3", now), MilliSeconds(Seconds(60)));
    EXPECT_EQ(limiter.blockedFor("petrov", "198.51.100.3", now), MilliSeconds::zero());

    now += Seconds(60);
    EXPECT_EQ(limiter.blockedFor("ivanov", "198.51.100.3", now), MilliSeconds::zero());

    // 120 s, 240 s, ...
    limiter.addFailure("ivanov", "198.51.100.4", now);
    EXPECT_EQ(limiter.blockedFor("ivanov", "198.51.100.3", now), MilliSeconds(Seconds(120)));
    now += Seconds(120);
    limiter.addFailure("ivanov", "198.51.100.5", now);
    EXPECT_EQ(limiter.blockedFor("ivanov", "198.51.100.3", now), MilliSeconds(Seconds(240)));
}

TEST(domain_auth_limiter_test, backoff_is_capped)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    for (int i = 0; i < 200; ++i)
    {
        limiter.addFailure("ivanov", QString("198.51.100.%1").arg(i % 250), now);
        now += Minutes(31);
        // Never permanently locked: after the maximum delay the account is free again.
        EXPECT_EQ(limiter.blockedFor("ivanov", "203.0.113.1", now), MilliSeconds::zero());
    }

    limiter.addFailure("ivanov", "198.51.100.1", now);
    EXPECT_EQ(limiter.blockedFor("ivanov", "203.0.113.1", now), MilliSeconds(Minutes(30)));
}

TEST(domain_auth_limiter_test, failures_are_forgotten)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    limiter.addFailure("ivanov", "198.51.100.1", now);
    limiter.addFailure("ivanov", "198.51.100.2", now);

    now += Hours(2);
    limiter.addFailure("ivanov", "198.51.100.3", now);
    EXPECT_EQ(limiter.blockedFor("ivanov", "203.0.113.1", now), MilliSeconds::zero());
}

TEST(domain_auth_limiter_test, address_is_limited_across_accounts)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    for (int i = 0; i < 4; ++i)
        limiter.addFailure(QString("user%1").arg(i), "2001:db8:1:2::1", now);
    EXPECT_EQ(limiter.blockedFor("other", "2001:db8:1:2::99", now), MilliSeconds::zero());

    // The fifth failure from the same /64 blocks every account from it.
    limiter.addFailure("user5", "2001:db8:1:2:ffff::1", now);
    EXPECT_EQ(limiter.blockedFor("other", "2001:db8:1:2::99", now), MilliSeconds(Seconds(30)));
    EXPECT_EQ(limiter.blockedFor("other", "2001:db8:1:3::1", now), MilliSeconds::zero());
}

TEST(domain_auth_limiter_test, success_resets)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    for (int i = 0; i < 3; ++i)
        limiter.addFailure("ivanov", kAddress, now);
    EXPECT_GT(limiter.blockedFor("ivanov", kAddress, now), MilliSeconds::zero());

    limiter.addSuccess("CORP\\Ivanov", kAddress);
    EXPECT_EQ(limiter.blockedFor("ivanov", kAddress, now), MilliSeconds::zero());
    EXPECT_EQ(limiter.accountCount(), 0);
    EXPECT_EQ(limiter.addressCount(), 0);
}

TEST(domain_auth_limiter_test, memory_is_bounded)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 50);
    TimePoint now = Clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        limiter.addFailure(QString("user%1").arg(i), QString("10.0.%1.%2").arg(i / 250).arg(i % 250), now);
        now += MilliSeconds(1);
    }

    EXPECT_LE(limiter.accountCount(), 50);
    EXPECT_LE(limiter.addressCount(), 50);

    // The newest keys are kept.
    for (int i = 0; i < 2; ++i)
        limiter.addFailure("user999", "10.0.3.249", now);
    EXPECT_GT(limiter.blockedFor("user999", "203.0.113.1", now), MilliSeconds::zero());
}

TEST(domain_auth_limiter_test, account_key_unicode)
{
    // Full-width letters and a full-width backslash / at sign are the same account (NFKC).
    EXPECT_EQ(DomainAuthLimiter::accountKey(QString::fromUtf8("ＩＶＡＮＯＶ")), "ivanov");
    EXPECT_EQ(DomainAuthLimiter::accountKey(QString::fromUtf8("CORP＼ivanov")), "ivanov");
    EXPECT_EQ(DomainAuthLimiter::accountKey(QString::fromUtf8("ivanov＠corp")), "ivanov");
    // Case folding beyond ASCII.
    EXPECT_EQ(DomainAuthLimiter::accountKey(QString::fromUtf8("ИВАНОВ")),
              DomainAuthLimiter::accountKey(QString::fromUtf8("иванов")));
    EXPECT_EQ(DomainAuthLimiter::accountKey("ivanov @corp"), "ivanov");
}

TEST(domain_auth_limiter_test, one_logon_per_account_at_a_time)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100, 2);
    TimePoint now = Clock::now();

    DomainAuthLimiter::Ticket first;
    DomainAuthLimiter::Ticket second;
    EXPECT_EQ(limiter.acquire("CORP\\ivanov", "198.51.100.1", now, &first),
              DomainAuthLimiter::Decision::ALLOWED);
    // Another spelling of the same account from another source waits.
    EXPECT_EQ(limiter.acquire("ivanov@corp", "198.51.100.2", now, &second),
              DomainAuthLimiter::Decision::BUSY);
    EXPECT_FALSE(second.valid);

    limiter.release(&first, DomainAuthLimiter::Outcome::FAILURE, now);
    EXPECT_FALSE(first.valid);
    EXPECT_EQ(limiter.acquire("ivanov", "198.51.100.2", now, &second),
              DomainAuthLimiter::Decision::ALLOWED);
    limiter.release(&second, DomainAuthLimiter::Outcome::CANCELLED, now);
}

TEST(domain_auth_limiter_test, in_flight_per_address)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100, 2);
    TimePoint now = Clock::now();

    DomainAuthLimiter::Ticket t[3];
    EXPECT_EQ(limiter.acquire("a", "2001:db8::1", now, &t[0]), DomainAuthLimiter::Decision::ALLOWED);
    EXPECT_EQ(limiter.acquire("b", "2001:db8::2", now, &t[1]), DomainAuthLimiter::Decision::ALLOWED);
    EXPECT_EQ(limiter.acquire("c", "2001:db8::3", now, &t[2]), DomainAuthLimiter::Decision::BUSY);
    EXPECT_EQ(limiter.acquire("c", "2001:db8:0:1::3", now, &t[2]), DomainAuthLimiter::Decision::ALLOWED);
    for (auto& ticket : t)
        limiter.release(&ticket, DomainAuthLimiter::Outcome::CANCELLED, now);
    EXPECT_EQ(limiter.accountCount(), 0);
    EXPECT_EQ(limiter.addressCount(), 0);
}

TEST(domain_auth_limiter_test, parallel_attempts_are_serialized)
{
    // Many threads try the same account at once: only one is ever in progress, and once the
    // account is blocked nobody passes. Without an atomic reservation all of them would pass.
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 1000, 1000);
    const TimePoint now = Clock::now();

    std::atomic_int in_progress = 0;
    std::atomic_int max_in_progress = 0;
    std::atomic_int allowed = 0;

    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i)
    {
        threads.emplace_back([&, i]()
        {
            for (int j = 0; j < 200; ++j)
            {
                DomainAuthLimiter::Ticket ticket;
                const QString address = QString("10.1.%1.%2").arg(i).arg(j % 250);
                if (limiter.acquire("victim", address, now, &ticket) !=
                    DomainAuthLimiter::Decision::ALLOWED)
                {
                    continue;
                }

                const int current = ++in_progress;
                int expected = max_in_progress.load();
                while (current > expected && !max_in_progress.compare_exchange_weak(expected, current)) {}
                ++allowed;
                std::this_thread::yield();
                --in_progress;
                limiter.release(&ticket, DomainAuthLimiter::Outcome::FAILURE, now);
            }
        });
    }
    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(max_in_progress.load(), 1);
    // The 3rd failure blocks the account (the clock does not move here).
    EXPECT_EQ(allowed.load(), kAccountPolicy.free_failures);
}

TEST(domain_auth_limiter_test, trusted_address_is_not_locked_out)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    DomainAuthLimiter::Ticket ticket;
    ASSERT_EQ(limiter.acquire("ivanov", "192.0.2.1", now, &ticket), DomainAuthLimiter::Decision::ALLOWED);
    limiter.release(&ticket, DomainAuthLimiter::Outcome::SUCCESS, now);

    // An attacker elsewhere locks the account.
    for (int i = 0; i < 3; ++i)
        limiter.addFailure("ivanov", QString("203.0.113.%1").arg(i), now);
    EXPECT_GT(limiter.blockedFor("ivanov", "203.0.113.50", now), MilliSeconds::zero());

    // The owner, from the address of the last successful logon, is not blocked.
    EXPECT_EQ(limiter.blockedFor("ivanov", "192.0.2.1", now), MilliSeconds::zero());
    EXPECT_EQ(limiter.acquire("CORP\\Ivanov", "192.0.2.1", now, &ticket),
              DomainAuthLimiter::Decision::ALLOWED);
    limiter.release(&ticket, DomainAuthLimiter::Outcome::CANCELLED, now);

    // But the source limits still apply to it.
    for (int i = 0; i < 5; ++i)
        limiter.addFailure(QString("x%1").arg(i), "192.0.2.1", now);
    EXPECT_GT(limiter.blockedFor("ivanov", "192.0.2.1", now), MilliSeconds::zero());

    // The trust expires.
    now += DomainAuthLimiter::kDefaultTrustPeriod + Hours(2);
    for (int i = 0; i < 3; ++i)
        limiter.addFailure("ivanov", QString("203.0.113.%1").arg(i), now);
    EXPECT_GT(limiter.blockedFor("ivanov", "192.0.2.1", now), MilliSeconds::zero());
}

TEST(domain_auth_limiter_test, eviction_keeps_active_blocks)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 10);
    TimePoint now = Clock::now();

    // Block "victim".
    for (int i = 0; i < 3; ++i)
        limiter.addFailure("victim", QString("198.51.100.%1").arg(i), now);
    ASSERT_GT(limiter.blockedFor("victim", "203.0.113.1", now), MilliSeconds::zero());

    // Flood with distinct names.
    for (int i = 0; i < 5000; ++i)
    {
        limiter.addFailure(QString("flood%1").arg(i), QString("10.2.%1.%2").arg(i / 250).arg(i % 250), now);
        now += MilliSeconds(1);
    }

    EXPECT_LE(limiter.accountCount(), 10);
    EXPECT_GT(limiter.blockedFor("victim", "203.0.113.1", now), MilliSeconds::zero());
}

TEST(domain_auth_limiter_test, full_of_blocks_refuses_new_keys)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 2);
    TimePoint now = Clock::now();

    for (const char* name : { "a", "b" })
    {
        for (int i = 0; i < 3; ++i)
            limiter.addFailure(name, QString("198.51.100.%1").arg(i * 10 + (name[0] - 'a')), now);
    }

    DomainAuthLimiter::Ticket ticket;
    EXPECT_EQ(limiter.acquire("c", "203.0.113.200", now, &ticket), DomainAuthLimiter::Decision::BUSY);
    EXPECT_EQ(limiter.accountCount(), 2);
}

TEST(domain_auth_limiter_test, address_failure)
{
    DomainAuthLimiter limiter(kAccountPolicy, kAddressPolicy, 100);
    TimePoint now = Clock::now();

    for (int i = 0; i < 5; ++i)
        limiter.addAddressFailure("192.0.2.77", now);
    EXPECT_GT(limiter.blockedFor("anyone", "192.0.2.77", now), MilliSeconds::zero());
    EXPECT_EQ(limiter.accountCount(), 0);
}
