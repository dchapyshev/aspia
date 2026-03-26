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

#include "base/net/anti_replay_window.h"

namespace base {

//--------------------------------------------------------------------------------------------------
bool AntiReplayWindow::check(quint64 counter)
{
    // Counter 0 is never valid (counters start from 1).
    if (counter == 0)
        return false;

    if (counter > max_counter_)
    {
        // New maximum. Shift the window forward.
        quint64 diff = counter - max_counter_;

        if (diff >= kWindowSize)
        {
            // The jump is larger than the window - reset everything.
            bitmap_.reset();
        }
        else
        {
            // Shift the bitmap: mark positions that fall out of the window as unseen.
            for (quint64 i = 0; i < diff; ++i)
            {
                quint64 bit_index = (max_counter_ + 1 + i) % kWindowSize;
                bitmap_.reset(static_cast<size_t>(bit_index));
            }
        }

        max_counter_ = counter;
        bitmap_.set(static_cast<size_t>(counter % kWindowSize));
        return true;
    }

    // Counter is within or behind the window.
    if (max_counter_ - counter >= kWindowSize)
        return false; // Too old - outside the window.

    size_t bit_index = static_cast<size_t>(counter % kWindowSize);

    if (bitmap_.test(bit_index))
        return false; // Already seen - replay.

    bitmap_.set(bit_index);
    return true;
}

//--------------------------------------------------------------------------------------------------
void AntiReplayWindow::reset()
{
    max_counter_ = 0;
    bitmap_.reset();
}

} // namespace base
