//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <gtest/gtest.h>

#include "build/build_config.h"
#include "console/computer_address.h"

namespace console {

TEST(ComputerAddressTest, Port)
{
    ComputerAddress addr1 = ComputerAddress::fromStdString("192.168.0.1:0");
    EXPECT_FALSE(addr1.isValid());
    EXPECT_TRUE(addr1.host().isEmpty());
    EXPECT_EQ(addr1.port(), 0);

    ComputerAddress addr2 = ComputerAddress::fromStdString("192.168.0.1:08051");
    EXPECT_TRUE(addr2.isValid());
    EXPECT_EQ(addr2.host(), "192.168.0.1");
    EXPECT_EQ(addr2.port(), 8051);

    ComputerAddress addr3 = ComputerAddress::fromStdString("192.168.0.1:FFFF");
    EXPECT_FALSE(addr3.isValid());
    EXPECT_TRUE(addr3.host().isEmpty());
    EXPECT_EQ(addr3.port(), 0);

    ComputerAddress addr4 = ComputerAddress::fromStdString("192.168.0.1:");
    EXPECT_FALSE(addr4.isValid());
    EXPECT_TRUE(addr4.host().isEmpty());
    EXPECT_EQ(addr4.port(), 0);
}

TEST(ComputerAddressTest, InvalidAddress)
{
    ComputerAddress addr1 = ComputerAddress::fromStdString("http://test.com");
    EXPECT_FALSE(addr1.isValid());
    EXPECT_TRUE(addr1.host().isEmpty());
    EXPECT_EQ(addr1.port(), 0);

    EXPECT_FALSE(ComputerAddress::fromStdString("https://test1.org").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("ftp://test2.net").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("te%st2").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("//").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("..").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString(".").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("\\").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("/").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString(":").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("^").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("#").isValid());
    EXPECT_FALSE(ComputerAddress::fromStdString("2001:db8:1f70::999:de8:7648:6e8").isValid());
}

TEST(ComputerAddressTest, ValidAddress)
{
    ComputerAddress addr1 = ComputerAddress::fromStdString("192.168.1.1");
    EXPECT_TRUE(addr1.isValid());
    EXPECT_EQ(addr1.host(), "192.168.1.1");
    EXPECT_EQ(addr1.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr1.toStdString(), "192.168.1.1");

    ComputerAddress addr2 = ComputerAddress::fromStdString("192.168.1.1:8080");
    EXPECT_TRUE(addr2.isValid());
    EXPECT_EQ(addr2.host(), "192.168.1.1");
    EXPECT_EQ(addr2.port(), 8080);
    EXPECT_EQ(addr2.toStdString(), "192.168.1.1:8080");

    ComputerAddress addr3 = ComputerAddress::fromStdString("test.com");
    EXPECT_TRUE(addr3.isValid());
    EXPECT_EQ(addr3.host(), "test.com");
    EXPECT_EQ(addr3.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr3.toStdString(), "test.com");

    ComputerAddress addr4 = ComputerAddress::fromStdString("test.com:8080");
    EXPECT_TRUE(addr4.isValid());
    EXPECT_EQ(addr4.host(), "test.com");
    EXPECT_EQ(addr4.port(), 8080);
    EXPECT_EQ(addr4.toStdString(), "test.com:8080");

    ComputerAddress addr5 = ComputerAddress::fromStdString("test");
    EXPECT_TRUE(addr5.isValid());
    EXPECT_EQ(addr5.host(), "test");
    EXPECT_EQ(addr5.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr5.toStdString(), "test");

    ComputerAddress addr6 = ComputerAddress::fromStdString("test:8080");
    EXPECT_TRUE(addr6.isValid());
    EXPECT_EQ(addr6.host(), "test");
    EXPECT_EQ(addr6.port(), 8080);
    EXPECT_EQ(addr6.toStdString(), "test:8080");

    ComputerAddress addr7 = ComputerAddress::fromStdString("[2001:db8:1f70::999:de8:7648:6e8]");
    EXPECT_TRUE(addr7.isValid());
    EXPECT_EQ(addr7.host(), "2001:db8:1f70::999:de8:7648:6e8");
    EXPECT_EQ(addr7.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr7.toStdString(), "[2001:db8:1f70::999:de8:7648:6e8]");

    ComputerAddress addr8 = ComputerAddress::fromStdString("[2001:db8:1f70::999:de8:7648:6e8]:8080");
    EXPECT_TRUE(addr8.isValid());
    EXPECT_EQ(addr8.host(), "2001:db8:1f70::999:de8:7648:6e8");
    EXPECT_EQ(addr8.port(), 8080);
    EXPECT_EQ(addr8.toStdString(), "[2001:db8:1f70::999:de8:7648:6e8]:8080");

    ComputerAddress addr9 = ComputerAddress::fromStdString("[::ffff:192.0.2.1]");
    EXPECT_TRUE(addr9.isValid());
    EXPECT_EQ(addr9.host(), "::ffff:192.0.2.1");
    EXPECT_EQ(addr9.port(), DEFAULT_HOST_TCP_PORT);
    EXPECT_EQ(addr9.toStdString(), "[::ffff:192.0.2.1]");

    ComputerAddress addr10 = ComputerAddress::fromStdString("[::ffff:192.0.2.1]:8080");
    EXPECT_TRUE(addr10.isValid());
    EXPECT_EQ(addr10.host(), "::ffff:192.0.2.1");
    EXPECT_EQ(addr10.port(), 8080);
    EXPECT_EQ(addr10.toStdString(), "[::ffff:192.0.2.1]:8080");
}

TEST(ComputerAddressTest, TestVector)
{
    static_assert(DEFAULT_HOST_TCP_PORT == 8050);

    ComputerAddress addr = ComputerAddress::fromStdString("192.168.1.1:8050");

    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.host(), "192.168.1.1");
    EXPECT_EQ(addr.port(), 8050);
    EXPECT_EQ(addr.toStdString(), "192.168.1.1");

    addr.setPort(8051);

    EXPECT_EQ(addr.toStdString(), "192.168.1.1:8051");

    addr.setHost("::ffff:192.0.2.1");

    EXPECT_EQ(addr.toStdString(), "[::ffff:192.0.2.1]:8051");

    addr.setPort(8050);

    EXPECT_EQ(addr.toStdString(), "[::ffff:192.0.2.1]");

    addr.setPort(0);
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toStdString().empty());

    addr.setHost("..");
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toStdString().empty());

    addr.setPort(8050);
    EXPECT_FALSE(addr.isValid());
    EXPECT_TRUE(addr.toStdString().empty());

    addr.setHost("192.168.1.1");
    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.host(), "192.168.1.1");
    EXPECT_EQ(addr.port(), 8050);
    EXPECT_EQ(addr.toStdString(), "192.168.1.1");
}

} // namespace console
