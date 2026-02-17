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

#ifndef BASE_CRYPTO_RANDOM_H
#define BASE_CRYPTO_RANDOM_H

#include <QByteArray>

namespace base {

class Random
{
public:
    // Fills buffer |buffer| of size |size| with random values.
    // If successful, returns |true| otherwise |false|.
    static bool fillBuffer(void* buffer, size_t size);

    // Generates a random buffer of size |size|.
    static QByteArray byteArray(size_t size);
    static std::string string(size_t size);

    // Generates a random number.
    static quint32 number32();
    static quint64 number64();

private:
    Q_DISABLE_COPY(Random)
};

} // namespace base

#endif // BASE_CRYPTO_RANDOM_H
