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

#include "base/codec/weighted_samples.h"

namespace base {

WeightedSamples::WeightedSamples(double weight_factor)
    : weight_factor_(weight_factor)
{
    // Nothing
}

WeightedSamples::~WeightedSamples() = default;

void WeightedSamples::record(double value)
{
    weighted_sum_ *= weight_factor_;
    weighted_sum_ += value;
    weight_ *= weight_factor_;
    ++weight_;
}

double WeightedSamples::weightedAverage() const
{
    if (weight_ == 0)
        return 0;

    return weighted_sum_ / weight_;
}

} // namespace base
