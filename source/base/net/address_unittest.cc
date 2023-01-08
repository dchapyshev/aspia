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
#include "base/net/address.h"

#include <gtest/gtest.h>

namespace base {

TEST(AddressTest, Port)
{
    Address addr1 = Address::fromString(u"192.168.0.1:0", 8050);
    EXPECT_FALSE(addr1.isValid());
    EXPECT_TRUE(addr1.host().empty());
    EXPECT_EQ(addr1.port(), 0);

    Address addr2 = Address::fromString(u"192.168.0.1:08051", 8050);
    EXPECT_TRUE(addr2.isValid());
    EXPECT_EQ(addr2.host(), u"192.168.0.1");
    EXPECT_EQ(addr2.port(), 8051);

    Address addr3 = Address::fromString(u"192.168.0.1:FFFF", 8050);
    EXPECT_FALSE(addr3.isValid());
    EXPECT_TRUE(addr3.host().empty());
    EXPECT_EQ(addr3.port(), 0);

    Address addr4 = Address::fromString(u"192.168.0.1:", 8060);
    EXPECT_FALSE(addr4.isValid());
    EXPECT_TRUE(addr4.host().empty());
    EXPECT_EQ(addr4.port(), 0);

    Address addr5 = Address::fromString(u"192.168.0.1:83572576", 8050);
    EXPECT_FALSE(addr5.isValid());
    EXPECT_TRUE(addr5.host().empty());
    EXPECT_EQ(addr5.port(), 0);

    Address addr6 = Address::fromString(u"192.168.0.1:65535", 8050);
    EXPECT_TRUE(addr6.isValid());
    EXPECT_EQ(addr6.host(), u"192.168.0.1");
    EXPECT_EQ(addr6.port(), 65535);
}

TEST(AddressTest, InvalidAddress)
{
    Address addr1 = Address::fromString(u"http://test.com", 8050);
    EXPECT_FALSE(addr1.isValid());
    EXPECT_TRUE(addr1.host().empty());
    EXPECT_EQ(addr1.port(), 0);

    EXPECT_FALSE(Address::fromString(u"https://test1.org", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"ftp://test2.net", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"te%st2", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"//", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"..", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u".", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"\\", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"/", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u":", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"^", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"#", 8050).isValid());
    EXPECT_FALSE(Address::fromString(u"2001:db8:1f70::999:de8:7648:6e8", 8050).isValid());
}

TEST(AddressTest, ValidAddress)
{
    Address addr1 = Address::fromString(u"192.168.1.1", 8050);
    EXPECT_TRUE(addr1.isValid());
    EXPECT_EQ(addr1.host(), u"192.168.1.1");
    EXPECT_EQ(addr1.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr1.toString(), u"192.168.1.1");

    Address addr2 = Address::fromString(u"192.168.1.1:8080", 8050);
    EXPECT_TRUE(addr2.isValid());
    EXPECT_EQ(addr2.host(), u"192.168.1.1");
    EXPECT_EQ(addr2.port(), 8080);
    EXPECT_EQ(addr2.toString(), u"192.168.1.1:8080");

    Address addr3 = Address::fromString(u"test.com", 8050);
    EXPECT_TRUE(addr3.isValid());
    EXPECT_EQ(addr3.host(), u"test.com");
    EXPECT_EQ(addr3.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr3.toString(), u"test.com");

    Address addr4 = Address::fromString(u"test.com:8080", 8050);
    EXPECT_TRUE(addr4.isValid());
    EXPECT_EQ(addr4.host(), u"test.com");
    EXPECT_EQ(addr4.port(), 8080);
    EXPECT_EQ(addr4.toString(), u"test.com:8080");

    Address addr5 = Address::fromString(u"test", 8050);
    EXPECT_TRUE(addr5.isValid());
    EXPECT_EQ(addr5.host(), u"test");
    EXPECT_EQ(addr5.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr5.toString(), u"test");

    Address addr6 = Address::fromString(u"test:8080", 8050);
    EXPECT_TRUE(addr6.isValid());
    EXPECT_EQ(addr6.host(), u"test");
    EXPECT_EQ(addr6.port(), 8080);
    EXPECT_EQ(addr6.toString(), u"test:8080");

    Address addr7 = Address::fromString(u"[2001:db8:1f70::999:de8:7648:6e8]", 8050);
    EXPECT_TRUE(addr7.isValid());
    EXPECT_EQ(addr7.host(), u"2001:db8:1f70::999:de8:7648:6e8");
    EXPECT_EQ(addr7.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr7.toString(), u"[2001:db8:1f70::999:de8:7648:6e8]");

    Address addr8 = Address::fromString(u"[2001:db8:1f70::999:de8:7648:6e8]:8080", 8050);
    EXPECT_TRUE(addr8.isValid());
    EXPECT_EQ(addr8.host(), u"2001:db8:1f70::999:de8:7648:6e8");
    EXPECT_EQ(addr8.port(), 8080);
    EXPECT_EQ(addr8.toString(), u"[2001:db8:1f70::999:de8:7648:6e8]:8080");

    Address addr9 = Address::fromString(u"[::ffff:192.0.2.1]", 8050);
    EXPECT_TRUE(addr9.isValid());
    EXPECT_EQ(addr9.host(), u"::ffff:192.0.2.1");
    EXPECT_EQ(addr9.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr9.toString(), u"[::ffff:192.0.2.1]");

    Address addr10 = Address::fromString(u"[::ffff:192.0.2.1]:8080", 8050);
    EXPECT_TRUE(addr10.isValid());
    EXPECT_EQ(addr10.host(), u"::ffff:192.0.2.1");
    EXPECT_EQ(addr10.port(), 8080);
    EXPECT_EQ(addr10.toString(), u"[::ffff:192.0.2.1]:8080");
}

TEST(AddressTest, TestVector)
{
    static_assert(DEFAULT_HOST_TCP_PORT == 8050);

    Address addr = Address::fromString(u"192.168.1.1:8050", 8050);

    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.host(), u"192.168.1.1");
    EXPECT_EQ(addr.port(), 8050);
    EXPECT_EQ(addr.toString(), u"192.168.1.1");

    addr.setPort(8051);

    EXPECT_EQ(addr.toString(), u"192.168.1.1:8051");

    addr.setHost(u"::ffff:192.0.2.1");

    EXPECT_EQ(addr.toString(), u"[::ffff:192.0.2.1]:8051");

    addr.setPort(8050);

    EXPECT_EQ(addr.toString(), u"[::ffff:192.0.2.1]");

    addr.setPort(0);
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toString().empty());

    addr.setHost(u"..");
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toString().empty());

    addr.setPort(8050);
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toString().empty());

    addr.setHost(u"192.168.1.1");
    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.host(), u"192.168.1.1");
    EXPECT_EQ(addr.port(), 8050);
    EXPECT_EQ(addr.toString(), u"192.168.1.1");
}

} // namespace base
