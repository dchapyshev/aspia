//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/net_utils.h"

#include <gtest/gtest.h>

//--------------------------------------------------------------------------------------------------
// An unset setting is the documented way of saying "listen everywhere", so it passes.
TEST(NetUtilsTest, UnsetListenInterfaceIsValid)
{
    EXPECT_TRUE(NetUtils::isValidListenInterface(QString()));
    EXPECT_TRUE(NetUtils::isValidListenInterface(""));
}

//--------------------------------------------------------------------------------------------------
TEST(NetUtilsTest, ListenInterfaceAcceptsAddressLiterals)
{
    EXPECT_TRUE(NetUtils::isValidListenInterface("127.0.0.1"));
    EXPECT_TRUE(NetUtils::isValidListenInterface("0.0.0.0"));
    EXPECT_TRUE(NetUtils::isValidListenInterface("192.168.1.10"));
    EXPECT_TRUE(NetUtils::isValidListenInterface("::1"));
    EXPECT_TRUE(NetUtils::isValidListenInterface("::"));
}

//--------------------------------------------------------------------------------------------------
// Everything that is not an address is refused, so a listener never falls back to the wildcard on
// a mistyped setting: that would open the service on every interface of the machine while the
// configuration asks for one.
TEST(NetUtilsTest, MalformedListenInterfaceIsRefused)
{
    EXPECT_FALSE(NetUtils::isValidListenInterface("not-an-address"));
    EXPECT_FALSE(NetUtils::isValidListenInterface("192.168.0.256"));
    EXPECT_FALSE(NetUtils::isValidListenInterface("192.168.0.0/24"));

    // The name of an interface is not the address of one - a plausible mistake in a configuration
    // file, and one that must not pass for "every interface" either.
    EXPECT_FALSE(NetUtils::isValidListenInterface("eth0"));

    // A host name is not resolved here: a listener binds to an address, and resolving at bind time
    // would make the interface it ends up on depend on DNS.
    EXPECT_FALSE(NetUtils::isValidListenInterface("localhost"));

    // Whitespace is not trimmed away silently.
    EXPECT_FALSE(NetUtils::isValidListenInterface(" 127.0.0.1"));
    EXPECT_FALSE(NetUtils::isValidListenInterface("   "));
}
