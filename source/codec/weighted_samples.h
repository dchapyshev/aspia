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

#ifndef CODEC__WEIGHTED_SAMPLES_H
#define CODEC__WEIGHTED_SAMPLES_H

#include <cstdint>

namespace codec {

// Aggregates the samples and gives each of them a weight based on its age. This class can help to
// smooth the input data.
class WeightedSamples final
{
public:
    explicit WeightedSamples(double weight_factor);
    ~WeightedSamples();

    void record(double value);
    double weightedAverage() const;

private:
    const double weight_factor_;
    double weighted_sum_ = 0;
    double weight_ = 0;
};

} // namespace codec

#endif // CODEC__WEIGHTED_SAMPLES_H
