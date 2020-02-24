//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/message_loop/pending_task.h"

namespace base {

PendingTask::PendingTask(
    Callback&& callback, TimePoint delayed_run_time, bool nestable, int sequence_num)
    : callback(std::move(callback)),
      delayed_run_time(delayed_run_time),
      sequence_num(sequence_num),
      nestable(nestable)
{
    // Nothing
}

bool PendingTask::operator<(const PendingTask& other) const
{
    // Since the top of a priority queue is defined as the "greatest" element, we need to invert
    // the comparison here. We want the smaller time to be at the top of the heap.

    if (delayed_run_time < other.delayed_run_time)
        return false;

    if (delayed_run_time > other.delayed_run_time)
        return true;

    // If the times happen to match, then we use the sequence number to decide.
    // Compare the difference to support integer roll-over.
    return (sequence_num - other.sequence_num) > 0;
}

} // namespace base
