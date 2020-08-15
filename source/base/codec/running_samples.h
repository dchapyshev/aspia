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

#ifndef BASE__CODEC__RUNNING_SAMPLES_H
#define BASE__CODEC__RUNNING_SAMPLES_H

#include "base/macros_magic.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace base {

// Calculates the maximum or average of the most recent N recorded samples.
// This is typically used to smooth out random variation in point samples
// over bandwidth, frame rate, etc.
class RunningSamples
{
public:
    // Constructs a running sample helper that stores |window_size| most
    // recent samples.
    explicit RunningSamples(int window_size);
    virtual ~RunningSamples();

    // Records a point sample.
    void record(int64_t value);

    // Returns the average over up to |window_size| of the most recent samples.
    // 0 if no sample available
    double average() const;

    // Returns the max over up to |window_size| of the most recent samples.
    // 0 if no sample available
    int64_t max() const;

    // Whether there is at least one record.
    bool isEmpty() const;

private:
    // Stores the desired window size, as size_t to avoid casting when comparing
    // with the size of |data_points_|.
    const size_t window_size_;

    // Stores the |window_size| most recently recorded samples.
    std::vector<int64_t> data_points_;

    // Holds the sum of the samples in |data_points_|.
    int64_t sum_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RunningSamples);
};

} // namespace base

#endif // BASE__CODEC__RUNNING_SAMPLES_H
