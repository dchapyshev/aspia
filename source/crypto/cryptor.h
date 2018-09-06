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

#ifndef ASPIA_CRYPTO__CRYPTOR_H_
#define ASPIA_CRYPTO__CRYPTOR_H_

namespace aspia {

class Cryptor
{
public:
    virtual ~Cryptor() = default;

    virtual size_t encryptedDataSize(size_t in_size) = 0;
    virtual bool encrypt(const char* in, size_t in_size, char* out) = 0;

    virtual size_t decryptedDataSize(size_t in_size) = 0;
    virtual bool decrypt(const char* in, size_t in_size, char* out) = 0;
};

} // namespace aspia

#endif // ASPIA_CRYPTO__CRYPTOR_H_
