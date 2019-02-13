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

#ifndef HOST__PASSWORD_GENERATOR_H
#define HOST__PASSWORD_GENERATOR_H

#include <QString>

#include "base/macros_magic.h"

namespace host {

class PasswordGenerator
{
public:
    PasswordGenerator() = default;
    ~PasswordGenerator() = default;

    enum Characters
    {
        UPPER_CASE = 1,
        LOWER_CASE = 2,
        DIGITS     = 4
    };

    static const uint32_t kDefaultCharacters = UPPER_CASE | LOWER_CASE | DIGITS;
    static const int kDefaultLength = 8;

    void setCharacters(uint32_t value);
    uint32_t characters() const;

    void setLength(int value);
    int length() const;

    QString result() const;

private:
    uint32_t characters_ = kDefaultCharacters;
    int length_ = kDefaultLength;

    DISALLOW_COPY_AND_ASSIGN(PasswordGenerator);
};

} // namespace host

#endif // HOST__PASSWORD_GENERATOR_H
