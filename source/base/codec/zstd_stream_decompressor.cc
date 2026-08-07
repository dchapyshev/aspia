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

namespace {

// Upper bound on the match window the incoming stream is allowed to declare. The compressor on the
// other side runs at its default level, for which zstd uses a 2 MB window; 8 MB leaves room for the
// higher levels. Without this the limit is 128 MB, which a few bytes from the peer can request.
constexpr int kMaxWindowLog = 23;

} // namespace

//--------------------------------------------------------------------------------------------------
ZstdStreamDecompressor::ZstdStreamDecompressor()
    : stream_(ZSTD_createDStream()),
      buffer_(ZSTD_DStreamOutSize())
{
    ZSTD_initDStream(stream_.get());

    const size_t ret = ZSTD_DCtx_setParameter(stream_.get(), ZSTD_d_windowLogMax, kMaxWindowLog);
    if (ZSTD_isError(ret))
        LOG(ERROR) << "ZSTD_DCtx_setParameter failed:" << ZSTD_getErrorName(ret);
}

//--------------------------------------------------------------------------------------------------
ZstdStreamDecompressor::~ZstdStreamDecompressor() = default;

//--------------------------------------------------------------------------------------------------
QByteArray ZstdStreamDecompressor::decompress(std::string_view source, qint64 max_output_size)
{
    ZSTD_inBuffer input;
    input.src = source.data();
    input.size = source.size();
    input.pos = 0;

    QByteArray result;

    // A compressed chunk expands to far more than one output buffer holds, so the input can be
    // consumed while zstd still has data to hand over. Looping on the input alone loses that tail,
    // and a call that leaves room in the output buffer is what proves nothing is held back.
    bool output_full = false;

    do
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

        if (max_output_size != 0 && result.size() > max_output_size)
        {
            LOG(ERROR) << "Decompressed output exceeds the limit of" << max_output_size
                       << "bytes, aborting";
            return QByteArray();
        }

        output_full = (output.pos == output.size);
    }
    while (input.pos < input.size || output_full);

    return result;
}
