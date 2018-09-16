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

#include "codec/decompressor_zstd.h"

#include "base/logging.h"

namespace aspia {

DecompressorZstd::DecompressorZstd()
{
    stream_ = ZSTD_createDStream();
    CHECK(stream_);
    reset();
}

DecompressorZstd::~DecompressorZstd()
{
    ZSTD_freeDStream(stream_);
}

void DecompressorZstd::reset()
{
    size_t ret = ZSTD_initDStream(stream_);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);
}

bool DecompressorZstd::process(const uint8_t* input_data,
                               size_t input_size,
                               uint8_t* output_data,
                               size_t output_size,
                               size_t* consumed,
                               size_t* written)
{
    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    size_t ret = ZSTD_decompressStream(stream_, &output, &input);
    if (ZSTD_isError(ret))
    {
        LOG(LS_WARNING) << "ZSTD_decompressStream failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    *consumed = input.pos;
    *written = output.pos;

    if (input.pos >= input.size)
        return false;

    return true;
}

} // namespace aspia
