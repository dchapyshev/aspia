//
// PROJECT:         Aspia
// FILE:            codec/decompressor_zlib.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/decompressor_zlib.h"

#include <QDebug>

namespace aspia {

DecompressorZLIB::DecompressorZLIB()
{
    memset(&stream_, 0, sizeof(stream_));

    int ret = inflateInit(&stream_);
    Q_ASSERT(ret == Z_OK);
}

DecompressorZLIB::~DecompressorZLIB()
{
    int ret = inflateEnd(&stream_);
    Q_ASSERT(ret == Z_OK);
}

void DecompressorZLIB::reset()
{
    int ret = inflateReset(&stream_);
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
    stream_.avail_in  = static_cast<uint32_t>(input_size);
    stream_.next_in   = input_data;
    stream_.avail_out = static_cast<uint32_t>(output_size);
    stream_.next_out  = output_data;

    int ret = inflate(&stream_, Z_NO_FLUSH);
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
