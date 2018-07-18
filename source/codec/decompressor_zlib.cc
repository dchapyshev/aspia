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

#include "codec/decompressor_zlib.h"

#include <QDebug>

namespace aspia {

DecompressorZLIB::DecompressorZLIB()
{
    memset(&stream_, 0, sizeof(stream_));

    int ret = zng_inflateInit(&stream_);
    Q_ASSERT(ret == Z_OK);
}

DecompressorZLIB::~DecompressorZLIB()
{
    int ret = zng_inflateEnd(&stream_);
    Q_ASSERT(ret == Z_OK);
}

void DecompressorZLIB::reset()
{
    int ret = zng_inflateReset(&stream_);
    Q_ASSERT(ret == Z_OK);
}

bool DecompressorZLIB::process(const quint8* input_data,
                               size_t input_size,
                               quint8* output_data,
                               size_t output_size,
                               size_t* consumed,
                               size_t* written)
{
    Q_ASSERT(output_size != 0);

    // Setup I/O parameters.
    stream_.avail_in  = static_cast<quint32>(input_size);
    stream_.next_in   = input_data;
    stream_.avail_out = static_cast<quint32>(output_size);
    stream_.next_out  = output_data;

    int ret = zng_inflate(&stream_, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR)
        qWarning() << "zlib decompression failed: " << ret;

    *consumed = input_size - stream_.avail_in;
    *written = output_size - stream_.avail_out;

    //
    // Since we check that output is always greater than 0, the only
    // reason for us to get Z_BUF_ERROR is when zlib requires more input
    // data.
    //
    return (ret == Z_OK || ret == Z_BUF_ERROR);
}

} // namespace aspia
