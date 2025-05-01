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

#include "base/converter.h"

#include <gtest/gtest.h>

namespace base {

TEST(converter_test, for_bool)
{
    struct TestTable
    {
        const char* source_value;
        bool expected_has_value;
        bool expected_value;
    } const kTable[] =
    {
        { "true",      true,  true  },
        { "false",     true,  false },
        { "1",         true,  true  },
        { "0",         true,  false },
        { "",          false, false },
        { " ",         false, false },
        { "A",         false, false },
        { "String",    false, false },
        { " true",     true,  true  },
        { " false",    true,  false },
        { " 1",        true,  true  },
        { " 0",        true,  false },
        { "true ",     true,  true  },
        { "false ",    true,  false },
        { "1 ",        true,  true  },
        { "0 ",        true,  false },
        { " true ",    true,  true  },
        { " false ",   true,  false },
        { " 1 ",       true,  true  },
        { " 0 ",       true,  false },
        { "   true ",  true,  true  },
        { "   false ", true,  false },
        { "   1 ",     true,  true  },
        { "   0 ",     true,  false }
    };

    for (size_t i = 0; i < std::size(kTable); ++i)
    {
        std::optional<bool> result = Converter<bool>::get_value(kTable[i].source_value);

        EXPECT_EQ(result.has_value(), kTable[i].expected_has_value);

        if (result.has_value())
        {
            EXPECT_EQ(*result, kTable[i].expected_value);

            std::string string = Converter<bool>::set_value(*result);

            EXPECT_EQ(string, *result ? "true" : "false");
        }
    }
}

TEST(converter_test, for_string)
{
    struct TestTable
    {
        const char* source_value;
        bool expected_has_value;
        const char* expected_value;
    };

    const TestTable kTableForGet[] =
    {
        { "true",     true, "true"   },
        { "  true",   true, "true"   },
        { "  true  ", true, "true"   },
        { "",         false, nullptr },
        { "  ",       false, nullptr }
    };

    for (size_t i = 0; i < std::size(kTableForGet); ++i)
    {
        std::optional<std::string> result =
            Converter<std::string>::get_value(kTableForGet[i].source_value);

        EXPECT_EQ(result.has_value(), kTableForGet[i].expected_has_value);

        if (result.has_value())
            EXPECT_EQ(*result, kTableForGet[i].expected_value);
    }

    const TestTable kTableForSet[] =
    {
        { "true", true, "true" },
        { "",     true, ""     },
        { "   ",  true, "   "  }
    };

    for (size_t i = 0; i < std::size(kTableForSet); ++i)
    {
        std::string result = Converter<std::string>::set_value(kTableForSet[i].source_value);
        EXPECT_EQ(result, kTableForSet[i].expected_value);
    }
}

#if 0
TEST(converter_test, for_double)
{
    struct TestTable
    {
        const char* source_value;
        bool expected_has_value;
        double expected_value;
    } const kTable[] =
    {
        { "0.0",       true,  0.0 },
        { "0,0",       false, 0.0 },
        { "1.000",     true,  1.0 },
        { "  1.000",   true,  1.0 },
        { "  1.000  ", true,  1.0 }
    };

    for (size_t i = 0; i < std::size(kTable); ++i)
    {
        std::optional<double> result = Converter<double>::get_value(kTable[i].source_value);

        EXPECT_EQ(result.has_value(), kTable[i].expected_has_value);

        if (result.has_value())
        {
            EXPECT_EQ(*result, kTable[i].expected_value);
        }
    }
}
#endif

TEST(converter_test, for_quint16)
{
    struct TestTable
    {
        const char* source_value;
        bool expected_has_value;
        quint16 expected_value;
    } const kTable[] =
    {
        { "0",       true,  0  },
        { "000",     true,  0  },
        { "001",     true,  1  },
        { "0016",    true,  16 },
        { "   18  ", true,  18 },
        { "",        false, 0  },
        { "   ",     false, 0  }
    };

    for (size_t i = 0; i < std::size(kTable); ++i)
    {
        std::optional<quint16> result = Converter<quint16>::get_value(kTable[i].source_value);

        EXPECT_EQ(result.has_value(), kTable[i].expected_has_value);

        if (result.has_value())
        {
            EXPECT_EQ(*result, kTable[i].expected_value);
        }
    }
}

} // namespace base
