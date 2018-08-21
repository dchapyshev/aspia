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

#ifndef ASPIA_CRYPTO__SHA_H_
#define ASPIA_CRYPTO__SHA_H_

#include <string>

#include "base/macros_magic.h"

struct crypto_hash_sha256_state;
struct crypto_hash_sha512_state;

namespace aspia {

class Sha256
{
public:
    Sha256();
    ~Sha256();

    static std::string hash(const std::string& data);

    void addData(const void* data, size_t length);
    void addData(const std::string& data);

    std::string result() const;

private:
    std::unique_ptr<crypto_hash_sha256_state> state_;
    DISALLOW_COPY_AND_ASSIGN(Sha256);
};

class Sha512
{
public:
    Sha512();
    ~Sha512();

    static std::string hash(const std::string& data);

    void addData(const void* data, size_t length);
    void addData(const std::string& data);

    std::string result() const;

private:
    std::unique_ptr<crypto_hash_sha512_state> state_;
    DISALLOW_COPY_AND_ASSIGN(Sha512);
};

} // namespace aspia

#endif // ASPIA_CRYPTO__SHA_H_
