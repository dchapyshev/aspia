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

#include <limits>
#include <type_traits>

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

    // A UniformRandomBitGenerator backed by the cryptographic RNG, usable with std distributions
    // and algorithms (std::shuffle, std::uniform_int_distribution). |T| is the unsigned integer
    // type produced per invocation.
    template <typename T>
    class Generator
    {
    public:
        using result_type = T;

        static_assert(std::is_unsigned_v<result_type>,
                      "Random::Generator result_type must be an unsigned integer type");

        static constexpr result_type min() { return 0; }
        static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

        result_type operator()()
        {
            result_type value = 0;
            Random::fillBuffer(&value, sizeof(value));
            return value;
        }
    };

private:
    Q_DISABLE_COPY_MOVE(Random)
};

#endif // BASE_CRYPTO_RANDOM_H
