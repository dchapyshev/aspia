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

#include "codec/running_samples.h"

#include <algorithm>

#include "base/logging.h"

namespace codec {

RunningSamples::RunningSamples(int window_size)
    : window_size_(window_size)
{
    DCHECK_GT(window_size, 0);
    data_points_.reserve(window_size + 1);
}

RunningSamples::~RunningSamples() = default;

void RunningSamples::record(int64_t value)
{
    data_points_.push_back(value);
    sum_ += value;

    if (data_points_.size() > window_size_)
    {
        sum_ -= data_points_.front();
        data_points_.erase(data_points_.begin());
    }
}

double RunningSamples::average() const
{
    if (data_points_.empty())
        return 0;

    return static_cast<double>(sum_) / data_points_.size();
}

int64_t RunningSamples::max() const
{
    if (data_points_.empty())
        return 0;

    return *std::max_element(data_points_.begin(), data_points_.end());
}

bool RunningSamples::isEmpty() const
{
    return data_points_.empty();
}

} // namespace codec
