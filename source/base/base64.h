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

#ifndef BASE__BASE64_H
#define BASE__BASE64_H

#include "base/macros_magic.h"

#if defined(HAS_QT)
#include <QByteArray>
#endif // defined(HAS_QT)

#include <string>

namespace base {

class Base64
{
public:
    // Encodes the input string in base64. The encoding can be done in-place.
    static void encode(const std::string& input, std::string* output);

    // Encodes the input string in base64.
    static std::string encode(const std::string& input);

    // Decodes the base64 input string. Returns true if successful and false otherwise.
    // The output string is only modified if successful. The decoding can be done in-place.
    static bool decode(const std::string& input, std::string* output);

    // Decodes the base64 input string. If an error occurred during decoding, an empty string
    // is returned.
    static std::string decode(const std::string& input);

#if defined(HAS_QT)
    static void encodeByteArray(const QByteArray& input, QByteArray* output);
    static QByteArray encodeByteArray(const QByteArray& input);
    static bool decodeByteArray(const QByteArray& input, QByteArray* output);
    static QByteArray decodeByteArray(const QByteArray& input);
#endif // defined(HAS_QT)

private:
    DISALLOW_COPY_AND_ASSIGN(Base64);
};

} // namespace base

#endif // BASE__BASE64_H
