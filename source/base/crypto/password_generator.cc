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

#include "base/crypto/password_generator.h"

#include <algorithm>
#include <random>

namespace base {

const quint32 PasswordGenerator::kDefaultCharacters = UPPER_CASE | LOWER_CASE | DIGITS;
const QString::size_type PasswordGenerator::kDefaultLength = 8;

//--------------------------------------------------------------------------------------------------
void PasswordGenerator::setCharacters(quint32 value)
{
    if (value)
        characters_ = value;
}

//--------------------------------------------------------------------------------------------------
void PasswordGenerator::setLength(QString::size_type value)
{
    if (value)
        length_ = value;
}

//--------------------------------------------------------------------------------------------------
QString PasswordGenerator::result() const
{
    constexpr std::string_view lower_case = "abcdefghijklmnopqrstuvwxyz";
    constexpr std::string_view upper_case = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr std::string_view digits = "0123456789";

    size_t table_length = 0;

    if (characters_ & LOWER_CASE)
        table_length += lower_case.length();

    if (characters_ & UPPER_CASE)
        table_length += upper_case.length();

    if (characters_ & DIGITS)
        table_length += digits.length();

    std::string table;
    table.reserve(table_length);

    if (characters_ & LOWER_CASE)
        table.append(lower_case);

    if (characters_ & UPPER_CASE)
        table.append(upper_case);

    if (characters_ & DIGITS)
        table.append(digits);

    std::random_device random_device;
    std::mt19937 engine(random_device());

    std::shuffle(table.begin(), table.end(), engine);

    std::uniform_int_distribution<> uniform_distance(0, static_cast<int>(table.size() - 1));

    QString result;
    result.resize(length_);

    for (QString::size_type i = 0; i < length_; ++i)
        result[i] = table[static_cast<size_t>(uniform_distance(engine))];

    return result;
}

} // namespace base
