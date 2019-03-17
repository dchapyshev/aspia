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

#include "host/password_generator.h"

#if defined(USE_PCG_GENERATOR)
#include <pcg_random.hpp>
#endif // defined(USE_PCG_GENERATOR)

#include <algorithm>
#include <random>
#include <vector>

namespace host {

void PasswordGenerator::setCharacters(uint32_t value)
{
    if (!value)
        return;

    characters_ = value;
}

uint32_t PasswordGenerator::characters() const
{
    return characters_;
}

void PasswordGenerator::setLength(int value)
{
    if (value <= 0)
        return;

    length_ = value;
}

int PasswordGenerator::length() const
{
    return length_;
}

std::string PasswordGenerator::result() const
{
    std::vector<char> table;

    if (characters_ & LOWER_CASE)
    {
        for (char i = 'a'; i < 'z'; ++i)
            table.push_back(i);
    }

    if (characters_ & UPPER_CASE)
    {
        for (char i = 'A'; i < 'Z'; ++i)
            table.push_back(i);
    }

    if (characters_ & DIGITS)
    {
        for (char i = '0'; i < '9'; ++i)
            table.push_back(i);
    }

#if defined(USE_PCG_GENERATOR)
    pcg_extras::seed_seq_from<std::random_device> random_device;
    pcg32 enigne(random_device);

    pcg_extras::shuffle(table.begin(), table.end(), enigne);
#else // defined(USE_PCG_GENERATOR)
    std::random_device random_device;
    std::mt19937 enigne(random_device());

    std::shuffle(table.begin(), table.end(), enigne);
#endif

    std::uniform_int_distribution<> uniform_distance(0, table.size() - 1);
    std::string result;

    for (int i = 0; i < length_; ++i)
        result += table[uniform_distance(enigne)];

    return result;
}

} // namespace host
