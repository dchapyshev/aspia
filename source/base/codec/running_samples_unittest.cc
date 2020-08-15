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

#include "base/codec/running_samples.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace base {

typedef void(*TestFunction)(size_t i, RunningSamples& samples);

static const int64_t kTestValues[] = { 10, 20, 30, 10, 25, 16, 15 };

// Test framework that verifies average() and max() at beginning, iterates
// through all elements and meanwhile calls your own test function
static void TestFramework(int windowSize, TestFunction testFn)
{
    RunningSamples samples(windowSize);
    EXPECT_EQ(0, samples.average());
    EXPECT_EQ(0, samples.max());

    for (size_t i = 0; i < std::size(kTestValues); ++i)
    {
        samples.record(kTestValues[i]);
        testFn(i, samples);
    }
}

// Average across a single element, i.e. just return the most recent.
TEST(RunningSamplesTest, AverageOneElementWindow)
{
    TestFramework(1, [](size_t i, RunningSamples& samples)
    {
        EXPECT_EQ(static_cast<double>(kTestValues[i]), samples.average());
    });
}

// Average the two most recent elements.
TEST(RunningSamplesTest, AverageTwoElementWindow)
{
    TestFramework(2, [](size_t i, RunningSamples& samples)
    {
        double expected = kTestValues[i];
        if (i > 0)
            expected = (expected + kTestValues[i - 1]) / 2;

        EXPECT_EQ(expected, samples.average());
    });
}

// Average across all the elements if the window size exceeds the element count.
TEST(RunningSamplesTest, AverageLongWindow)
{
    TestFramework(std::size(kTestValues) + 1, [](size_t i, RunningSamples& samples)
    {
        double expected = 0.0;
        for (size_t j = 0; j <= i; ++j)
            expected += kTestValues[j];
        expected /= i + 1;

        EXPECT_EQ(expected, samples.average());
    });
}

// Max of a single element, i.e. just return the most recent.
TEST(RunningSamplesTest, MaxOneElementWindow)
{
    TestFramework(1, [](size_t i, RunningSamples& samples)
    {
        EXPECT_EQ(static_cast<double>(kTestValues[i]), samples.max());
    });
}

// Max of the two most recent elements.
TEST(RunningSamplesTest, MaxTwoElementWindow)
{
    TestFramework(2, [](size_t i, RunningSamples& samples)
    {
        double expected = kTestValues[i];
        if (i > 0)
            expected = expected > kTestValues[i - 1] ? expected : kTestValues[i - 1];

        EXPECT_EQ(expected, samples.max());
    });
}

// Max of all the elements if the window size exceeds the element count.
TEST(RunningSamplesTest, MaxLongWindow)
{
    TestFramework(std::size(kTestValues) + 1, [](size_t i, RunningSamples& samples)
    {
        int64_t expected = -1;
        for (size_t j = 0; j <= i; ++j)
            expected = expected > kTestValues[j] ? expected : kTestValues[j];

        EXPECT_EQ(expected, samples.max());
    });
}

} // namespace base
