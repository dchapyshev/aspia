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

#include "codec/weighted_samples.h"

#include "base/stl_util.h"

#include <gtest/gtest.h>

namespace codec {

TEST(WeightedSamplesTest, CalculateWeightedAverage)
{
    static constexpr double kWeightFactor = 0.9;
    static constexpr double kExpected[] =
    {
        1,
        1.5263157894736843,
        2.0701107011070110,
        2.6312881651642916,
    };
    WeightedSamples samples(kWeightFactor);
    for (size_t i = 0; i < std::size(kExpected); i++)
    {
        samples.record(i + 1);
        EXPECT_DOUBLE_EQ(kExpected[i], samples.weightedAverage());
    }
}

TEST(WeightedSamplesTest, CalculateWeightedAverage_SameValues)
{
    WeightedSamples samples(0.9);
    for (int i = 0; i < 100; i++)
    {
        samples.record(100);
    }
    EXPECT_EQ(samples.weightedAverage(), 100);
}

TEST(WeightedSamplesTest, ReturnZeroIfNoRecords)
{
    WeightedSamples samples(0.9);
    EXPECT_EQ(samples.weightedAverage(), 0);
}

} // namespace codec
