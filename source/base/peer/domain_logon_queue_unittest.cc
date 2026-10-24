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

#include "base/peer/domain_logon_queue.h"

#include <gtest/gtest.h>

#include <QSemaphore>

TEST(domain_logon_queue_test, in_flight_cap)
{
    DomainLogonQueue queue(2, 3);
    QSemaphore release;
    std::atomic_int runs = 0;

    auto task = [&](bool cancelled)
    {
        EXPECT_FALSE(cancelled);
        release.acquire();
        ++runs;
    };

    EXPECT_TRUE(queue.start(nullptr, task));
    EXPECT_TRUE(queue.start(nullptr, task));
    EXPECT_TRUE(queue.start(nullptr, task));
    EXPECT_EQ(queue.inFlight(), 3);

    // Full: refused, the task is never called.
    EXPECT_FALSE(queue.start(nullptr, task));

    release.release(3);
    queue.waitForDone();
    EXPECT_EQ(runs.load(), 3);
    EXPECT_EQ(queue.inFlight(), 0);

    EXPECT_TRUE(queue.start(nullptr, [&](bool) { ++runs; }));
    queue.waitForDone();
    EXPECT_EQ(runs.load(), 4);
}

TEST(domain_logon_queue_test, cancelled_task_is_skipped)
{
    DomainLogonQueue queue(1, 4);
    QSemaphore release;
    std::atomic_int logons = 0;
    std::atomic_int skipped = 0;

    // The only thread is busy, so the next task waits in the queue.
    EXPECT_TRUE(queue.start(nullptr, [&](bool) { release.acquire(); }));

    auto cancel = std::make_shared<std::atomic_bool>(false);
    EXPECT_TRUE(queue.start(cancel, [&](bool cancelled)
    {
        if (cancelled)
            ++skipped;
        else
            ++logons;
    }));

    // The connection is gone before the logon started.
    cancel->store(true);
    release.release();
    queue.waitForDone();

    EXPECT_EQ(logons.load(), 0);
    EXPECT_EQ(skipped.load(), 1);
    EXPECT_EQ(queue.inFlight(), 0);
}

TEST(domain_logon_queue_test, empty_task_is_refused)
{
    DomainLogonQueue queue(1, 1);
    EXPECT_FALSE(queue.start(nullptr, DomainLogonQueue::Task()));
    EXPECT_EQ(queue.inFlight(), 0);
}
