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

#include "base/codec/zstd_stream_decompressor.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
ZstdStreamDecompressor::ZstdStreamDecompressor()
    : stream_(ZSTD_createDStream()),
      buffer_(ZSTD_DStreamOutSize())
{
    ZSTD_initDStream(stream_.get());
}

//--------------------------------------------------------------------------------------------------
ZstdStreamDecompressor::~ZstdStreamDecompressor() = default;

//--------------------------------------------------------------------------------------------------
QByteArray ZstdStreamDecompressor::decompress(std::string_view source)
{
    ZSTD_inBuffer input;
    input.src = source.data();
    input.size = source.size();
    input.pos = 0;

    QByteArray result;

    while (input.pos < input.size)
    {
        ZSTD_outBuffer output;
        output.dst = buffer_.data();
        output.size = buffer_.size();
        output.pos = 0;

        const size_t ret = ZSTD_decompressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(ERROR) << "ZSTD_decompressStream failed:" << ZSTD_getErrorName(ret);
            return QByteArray();
        }

        result.append(buffer_.data(), static_cast<int>(output.pos));
    }

    return result;
}
