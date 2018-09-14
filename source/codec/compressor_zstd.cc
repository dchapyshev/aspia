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

#include "codec/compressor_zstd.h"

#include "base/logging.h"

namespace aspia {

CompressorZstd::CompressorZstd(int compress_ratio)
    : compress_ratio_(compress_ratio)
{
    stream_ = ZSTD_createCStream();
    CHECK(stream_);
}

CompressorZstd::~CompressorZstd()
{
    ZSTD_freeCStream(stream_);
}

bool CompressorZstd::process(const uint8_t* input_data,
                             size_t input_size,
                             uint8_t* output_data,
                             size_t output_size,
                             CompressorFlush flush,
                             size_t* consumed,
                             size_t* written)
{
    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    size_t ret = ZSTD_compressStream(stream_, &output, &input);
    if (ZSTD_isError(ret))
    {
        LOG(LS_WARNING) << "ZSTD_compressStream failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    bool compress_again = true;

    if (input.pos == input.size)
        compress_again = false;

    if (!compress_again && flush == CompressorFinish)
    {
        ret = ZSTD_endStream(stream_, &output);
        if (ZSTD_isError(ret))
        {
            LOG(LS_WARNING) << "ZSTD_endStream failed: " << ZSTD_getErrorName(ret);
            return false;
        }
    }

    *consumed = input.pos;
    *written = output.pos;

    return compress_again;
}

void CompressorZstd::reset()
{
    size_t ret = ZSTD_initCStream(stream_, compress_ratio_);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);
}

} // namespace aspia
