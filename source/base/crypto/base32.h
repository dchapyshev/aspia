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

#ifndef BASE_CRYPTO_BASE32_H
#define BASE_CRYPTO_BASE32_H

#include <QByteArray>

class Base32
{
public:
    // Encode |data| using the RFC 4648 Base32 alphabet (A-Z, 2-7).
    // |with_padding| controls whether '=' padding characters are appended; otpauth:// URIs
    // are typically written without padding.
    static QByteArray encode(const QByteArray& data, bool with_padding = true);

    // Decode |data| accepting either upper- or lower-case characters and optional padding.
    // Returns an empty QByteArray if the input contains characters outside the alphabet or
    // has an illegal length (per RFC 4648 section 6).
    static QByteArray decode(const QByteArray& data, bool* ok = nullptr);

private:
    Q_DISABLE_COPY_MOVE(Base32)
};

#endif // BASE_CRYPTO_BASE32_H
