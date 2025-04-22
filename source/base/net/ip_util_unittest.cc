//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"
#include "base/net/ip_util.h"

#include <gtest/gtest.h>

namespace base {

TEST(IpUtilTest, Range)
{
    struct TestTable
    {
        const char* ip;
        const char* network;
        const char* mask;
        bool expected;
    } const kTestTable[] =
    {
        { "192.168.88.1", "192.168.88.0", "255.255.255.0", true },
        { "192.168.88.1", "192.168.88.2", "255.255.255.255", false },
        { "192.168.88.3", "192.168.88.2", "255.255.255.255", false },

        { "100.1.1.22", "192.168.1.0", "255.255.255.0", false },
        { "100.1.1.22", "100.1.1.22", "255.255.255.255", true },
        { "100.1.1.22", "100.1.1.23", "255.255.255.255", false },
        { "100.1.1.22", "100.1.1.21", "255.255.255.255", false },

        { "0.0.0.1", "0.0.0.0", "0.0.0.0", true },
        { "192.168.1.2", "10.0.0.1", "255.255.255.255", false }
    };

    for (size_t i = 0; i < std::size(kTestTable); ++i)
    {
        const auto& test_case = kTestTable[i];
        EXPECT_EQ(isIpInRange(test_case.ip, test_case.network, test_case.mask),
                  test_case.expected);
    }
}

} // namespace base
