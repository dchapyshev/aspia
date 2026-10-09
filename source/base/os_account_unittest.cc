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
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/os_account.h"

#include <gtest/gtest.h>

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, ValidNames)
{
    EXPECT_TRUE(OsAccount::isValidUserName("user"));
    EXPECT_TRUE(OsAccount::isValidUserName("first.last_name-2"));
    EXPECT_TRUE(OsAccount::isValidUserName("DOMAIN\\user"));
    EXPECT_TRUE(OsAccount::isValidUserName(".\\user"));
    EXPECT_TRUE(OsAccount::isValidUserName("\\user"));
    EXPECT_TRUE(OsAccount::isValidUserName("user@corp.local"));
    EXPECT_TRUE(OsAccount::isValidUserName("John Smith"));
    EXPECT_TRUE(OsAccount::isValidUserName(QString::fromUtf8("ДОМЕН\\Иван")));
}

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, EmptyUserIsInvalid)
{
    EXPECT_FALSE(OsAccount::isValidUserName(QString()));
    EXPECT_FALSE(OsAccount::isValidUserName("\\"));
    EXPECT_FALSE(OsAccount::isValidUserName("DOMAIN\\"));
}

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, OnlyOneDomainSeparator)
{
    EXPECT_FALSE(OsAccount::isValidUserName("DOMAIN\\user\\other"));
    EXPECT_FALSE(OsAccount::isValidUserName("DOMAIN\\\\user"));
}

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, InvalidCharacters)
{
    const QString invalid_chars("\"/[]:;|=,+*?<>");

    for (const QChar c : invalid_chars)
    {
        EXPECT_FALSE(OsAccount::isValidUserName(QString("us") + c + "er"))
            << "in user: " << QString(c).toStdString();
        EXPECT_FALSE(OsAccount::isValidUserName(QString("DO") + c + "MAIN\\user"))
            << "in domain: " << QString(c).toStdString();
    }
}

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, ControlCharacters)
{
    EXPECT_FALSE(OsAccount::isValidUserName(QString("us") + QChar(0x00) + "er"));
    EXPECT_FALSE(OsAccount::isValidUserName("us\ter"));
    EXPECT_FALSE(OsAccount::isValidUserName("us\ner"));
    EXPECT_FALSE(OsAccount::isValidUserName(QString("DO") + QChar(0x1f) + "MAIN\\user"));
}

//--------------------------------------------------------------------------------------------------
TEST(OsAccountTest, LengthOfEachPart)
{
    const QString longest(255, 'a');
    const QString too_long(256, 'a');

    EXPECT_TRUE(OsAccount::isValidUserName(longest));
    EXPECT_TRUE(OsAccount::isValidUserName(longest + "\\" + longest));
    EXPECT_FALSE(OsAccount::isValidUserName(too_long));
    EXPECT_FALSE(OsAccount::isValidUserName(too_long + "\\user"));
    EXPECT_FALSE(OsAccount::isValidUserName("DOMAIN\\" + too_long));
}
