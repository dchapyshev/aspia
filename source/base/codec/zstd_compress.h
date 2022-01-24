//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CODEC__ZSTD_COMPRESS_H
#define BASE__CODEC__ZSTD_COMPRESS_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"

namespace base {

class ZstdCompress
{
public:
    static const int kMinCompressLevel = 1;
    static const int kMaxCompressLevel = 22;
    static const int kDefCompressLevel = 8;

    static std::string compress(const std::string& source, int compress_level = kDefCompressLevel);
    static ByteArray compress(const ByteArray& source, int compress_level = kDefCompressLevel);

    static std::string decompress(const std::string& source);
    static ByteArray decompress(const ByteArray& source);

private:
    DISALLOW_COPY_AND_ASSIGN(ZstdCompress);
};

} // namespace base

#endif // BASE__CODEC__ZSTD_COMPRESS_H
