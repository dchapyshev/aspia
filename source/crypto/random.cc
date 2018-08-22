//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/random.h"

#define SODIUM_STATIC
#include <sodium.h>

namespace aspia {

// static
void Random::fillBuffer(void* buffer, size_t size)
{
    randombytes_buf(buffer, size);
}

// static
std::string Random::generateBuffer(size_t size)
{
    std::string random_buffer;
    random_buffer.resize(size);

    fillBuffer(random_buffer.data(), random_buffer.size());
    return random_buffer;
}

// static
uint32_t Random::generateNumber()
{
    return randombytes_random();
}

} // namespace aspia
