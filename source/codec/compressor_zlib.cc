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

#include "codec/compressor_zlib.h"

#include "base/logging.h"

namespace aspia {

CompressorZLIB::CompressorZLIB(int compress_ratio)
{
    memset(&stream_, 0, sizeof(stream_));

    int ret = zng_deflateInit2(&stream_,
                               compress_ratio,
                               Z_DEFLATED,
                               MAX_WBITS,
                               MAX_MEM_LEVEL,
                               Z_DEFAULT_STRATEGY);
    DCHECK_EQ(ret, Z_OK);
}

CompressorZLIB::~CompressorZLIB()
{
    int ret = zng_deflateEnd(&stream_);
    DCHECK_EQ(ret, Z_OK);
}

// static
int CompressorZLIB::minCompressRatio()
{
    return Z_BEST_SPEED;
}

// static
int CompressorZLIB::maxCompressRatio()
{
    return Z_BEST_COMPRESSION;
}

void CompressorZLIB::reset()
{
    int ret = zng_deflateReset(&stream_);
    DCHECK_EQ(ret, Z_OK);
}

bool CompressorZLIB::process(const uint8_t* input_data,
                             size_t input_size,
                             uint8_t* output_data,
                             size_t output_size,
                             CompressorFlush flush,
                             size_t* consumed,
                             size_t* written)
{
    DCHECK_NE(output_size, 0U);

    // Setup I/O parameters.
    stream_.avail_in  = static_cast<uint32_t>(input_size);
    stream_.next_in   = input_data;
    stream_.avail_out = static_cast<uint32_t>(output_size);
    stream_.next_out  = output_data;

    int z_flush = 0;

    switch (flush)
    {
        case CompressorSyncFlush:
            z_flush = Z_SYNC_FLUSH;
            break;

        case CompressorFinish:
            z_flush = Z_FINISH;
            break;

        case CompressorNoFlush:
            z_flush = Z_NO_FLUSH;
            break;

        default:
            LOG(LS_WARNING) << "Unsupported flush mode";
            break;
    }

    int ret = zng_deflate(&stream_, z_flush);
    if (ret == Z_STREAM_ERROR)
        LOG(LS_WARNING) << "zlib compression failed";

    *consumed = input_size - stream_.avail_in;
    *written = output_size - stream_.avail_out;

    //
    // If |ret| equals Z_STREAM_END we have reached the end of stream.
    // If |ret| equals Z_BUF_ERROR we have the end of the synchronication point.
    // For these two cases we need to stop compressing.
    //
    switch (ret)
    {
        case Z_OK:
            return true;

        case Z_STREAM_END:
            return false;

        case Z_BUF_ERROR:
            return stream_.avail_out == 0;

        default:
            LOG(LS_WARNING) << "Unexpected zlib error: " << ret;
            break;
    }

    return false;
}

size_t CompressorZLIB::compressBound(size_t input_size) const
{
    return input_size + (input_size / 100 + 16);
}

} // namespace aspia
