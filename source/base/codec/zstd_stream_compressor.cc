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

#include "base/codec/zstd_stream_compressor.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
ZstdStreamCompressor::ZstdStreamCompressor(int compression_level)
    : stream_(ZSTD_createCStream()),
      buffer_(ZSTD_CStreamOutSize())
{
    ZSTD_CCtx_setParameter(stream_.get(), ZSTD_c_compressionLevel, compression_level);
}

//--------------------------------------------------------------------------------------------------
ZstdStreamCompressor::~ZstdStreamCompressor() = default;

//--------------------------------------------------------------------------------------------------
std::string ZstdStreamCompressor::compress(QByteArrayView source)
{
    ZSTD_inBuffer input;
    input.src = source.data();
    input.size = static_cast<size_t>(source.size());
    input.pos = 0;

    std::string result;

    // Drive the stream with ZSTD_e_flush until both the input is consumed and the flush is complete
    // (a zero return), looping when the output buffer fills.
    size_t remaining;
    do
    {
        ZSTD_outBuffer output;
        output.dst = buffer_.data();
        output.size = buffer_.size();
        output.pos = 0;

        remaining = ZSTD_compressStream2(stream_.get(), &output, &input, ZSTD_e_flush);
        if (ZSTD_isError(remaining))
        {
            LOG(ERROR) << "ZSTD_compressStream2 failed:" << ZSTD_getErrorName(remaining);
            return std::string();
        }

        result.append(buffer_.data(), output.pos);
    }
    while (remaining != 0);

    return result;
}
