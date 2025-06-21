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

#ifndef BASE_CRYPTO_PASSWORD_GENERATOR_H
#define BASE_CRYPTO_PASSWORD_GENERATOR_H

#include <QString>

namespace base {

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

    static const quint32 kDefaultCharacters;
    static const qsizetype kDefaultLength;

    void setCharacters(quint32 value);
    quint32 characters() const { return characters_; }

    void setLength(qsizetype value);
    qsizetype length() const { return length_; }

    QString result() const;

private:
    quint32 characters_ = kDefaultCharacters;
    qsizetype length_ = kDefaultLength;

    Q_DISABLE_COPY(PasswordGenerator)
};

} // namespace base

#endif // BASE_CRYPTO_PASSWORD_GENERATOR_H
