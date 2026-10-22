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

#include <algorithm>

//--------------------------------------------------------------------------------------------------
DomainLogonQueue::DomainLogonQueue(int threads, int max_in_flight)
    : max_in_flight_(std::max(max_in_flight, 1))
{
    pool_.setMaxThreadCount(std::max(threads, 1));
    pool_.setObjectName(QStringLiteral("DomainLogonQueue"));
}

//--------------------------------------------------------------------------------------------------
DomainLogonQueue::~DomainLogonQueue()
{
    // Only queues made by tests are destroyed; their tasks are short.
    pool_.waitForDone();
}

//--------------------------------------------------------------------------------------------------
// static
DomainLogonQueue* DomainLogonQueue::instance()
{
    // Intentionally never destroyed: a logon may wait for an unreachable domain controller for a
    // long time, and the process must not hang on exit waiting for it (nor may a running task touch
    // a destroyed queue). The pool threads end with the process.
    static DomainLogonQueue* queue =
        new DomainLogonQueue(kDefaultThreads, kDefaultMaxInFlight);
    return queue;
}

//--------------------------------------------------------------------------------------------------
bool DomainLogonQueue::start(std::shared_ptr<std::atomic_bool> cancel, Task task)
{
    if (!task)
        return false;

    // Reserve a slot first, so that concurrent callers can not overshoot the limit.
    int current = in_flight_.load();
    do
    {
        if (current >= max_in_flight_)
            return false;
    }
    while (!in_flight_.compare_exchange_weak(current, current + 1));

    pool_.start([this, cancel = std::move(cancel), task = std::move(task)]()
    {
        const bool cancelled = cancel && cancel->load();
        task(cancelled);
        --in_flight_;
    });

    return true;
}

//--------------------------------------------------------------------------------------------------
void DomainLogonQueue::waitForDone()
{
    pool_.waitForDone();
}
