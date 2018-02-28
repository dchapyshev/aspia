//
// PROJECT:         Aspia
// FILE:            base/message_loop/pending_task.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/pending_task.h"

namespace aspia {

PendingTask::PendingTask(Callback callback)
    : callback(std::move(callback))
{
    // Nothing
}

PendingTask::PendingTask(
    Callback callback,
    const std::chrono::time_point<std::chrono::high_resolution_clock>& delayed_run_time)
    : callback(std::move(callback)),
      delayed_run_time(delayed_run_time)
{
    // Nothing
}

bool PendingTask::operator<(const PendingTask& other) const
{
    // Since the top of a priority queue is defined as the "greatest" element,
    // we need to invert the comparison here.  We want the smaller time to be
    // at the top of the heap.

    if (delayed_run_time < other.delayed_run_time)
        return false;

    if (delayed_run_time > other.delayed_run_time)
        return true;

    // If the times happen to match, then we use the sequence number to decide.
    // Compare the difference to support integer roll-over.
    return (sequence_num - other.sequence_num) > 0;
}

} // namespace aspia
