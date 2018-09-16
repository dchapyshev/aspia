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

#ifndef ASPIA_CODEC__SCOPED_ZSTD_DICT_H_
#define ASPIA_CODEC__SCOPED_ZSTD_DICT_H_

#include <memory>

#include <zstd.h>

namespace aspia {

struct ZstdCDictDeleter
{
    void operator()(ZSTD_CDict* cdict);
};

struct ZstdDDictDeleter
{
    void operator()(ZSTD_DDict* ddict);
};

using ScopedZstdCDict = std::unique_ptr<ZSTD_CDict, ZstdCDictDeleter>;
using ScopedZstdDDict = std::unique_ptr<ZSTD_DDict, ZstdDDictDeleter>;

} // namespace aspia

#endif // ASPIA_CODEC__SCOPED_ZSTD_DICT_H_
