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

#ifndef BASE_PEER_DOMAIN_LOGON_QUEUE_H
#define BASE_PEER_DOMAIN_LOGON_QUEUE_H

#include <QThreadPool>

#include <atomic>
#include <functional>
#include <memory>

// Runs operating system / domain logons (which block for seconds while a domain controller answers)
// on a small dedicated pool, so that they can not starve the global thread pool of the process.
//
// At most |max_in_flight| logons are queued or running at a time; start() refuses more. Every task
// has a cancellation flag: a logon whose connection is gone by the time a thread picks it up is
// skipped without asking the operating system.
class DomainLogonQueue
{
public:
    static constexpr int kDefaultThreads = 3;
    static constexpr int kDefaultMaxInFlight = 16;

    DomainLogonQueue(int threads, int max_in_flight);
    ~DomainLogonQueue();

    // The queue of the process.
    static DomainLogonQueue* instance();

    using Task = std::function<void(bool cancelled)>;

    // Queues |task|. The task is called on a pool thread with |cancelled| = true if |cancel| was set
    // before the task started (the task must then only release its resources). Returns false if the
    // queue is full; the task is not called then.
    bool start(std::shared_ptr<std::atomic_bool> cancel, Task task);

    int inFlight() const { return in_flight_.load(); }
    int maxInFlight() const { return max_in_flight_; }

    // Waits until all tasks have finished (for tests).
    void waitForDone();

private:
    QThreadPool pool_;
    const int max_in_flight_;
    std::atomic_int in_flight_ = 0;

    Q_DISABLE_COPY_MOVE(DomainLogonQueue)
};

#endif // BASE_PEER_DOMAIN_LOGON_QUEUE_H
