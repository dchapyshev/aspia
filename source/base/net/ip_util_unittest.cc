//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
        std::u16string_view ip;
        std::u16string_view network;
        std::u16string_view mask;
        bool expected;
    } const kTestTable[] =
    {
        { u"192.168.88.1", u"192.168.88.0", u"255.255.255.0", true },
        { u"192.168.88.1", u"192.168.88.2", u"255.255.255.255", false },
        { u"192.168.88.3", u"192.168.88.2", u"255.255.255.255", false },

        { u"100.1.1.22", u"192.168.1.0", u"255.255.255.0", false },
        { u"100.1.1.22", u"100.1.1.22", u"255.255.255.255", true },
        { u"100.1.1.22", u"100.1.1.23", u"255.255.255.255", false },
        { u"100.1.1.22", u"100.1.1.21", u"255.255.255.255", false },

        { u"0.0.0.1", u"0.0.0.0", u"0.0.0.0", true },
        { u"192.168.1.2", u"10.0.0.1", u"255.255.255.255", false }
    };

    for (size_t i = 0; i < std::size(kTestTable); ++i)
    {
        const auto& test_case = kTestTable[i];
        EXPECT_EQ(isIpInRange(test_case.ip, test_case.network, test_case.mask),
                  test_case.expected);
    }
}

} // namespace base
