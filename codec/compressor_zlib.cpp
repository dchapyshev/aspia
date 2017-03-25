//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/compressor_zlib.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/compressor_zlib.h"

#include "base/logging.h"

namespace aspia {

CompressorZLIB::CompressorZLIB(int32_t compress_ratio)
{
    memset(&stream_, 0, sizeof(stream_));

    int ret = deflateInit2(&stream_,
                           compress_ratio,
                           Z_DEFLATED,
                           MAX_WBITS,
                           MAX_MEM_LEVEL,
                           Z_DEFAULT_STRATEGY);
    DCHECK_EQ(ret, Z_OK);
}

CompressorZLIB::~CompressorZLIB()
{
    int ret = deflateEnd(&stream_);
    DCHECK_EQ(ret, Z_OK);
}

void CompressorZLIB::Reset()
{
    int ret = deflateReset(&stream_);
    DCHECK_EQ(ret, Z_OK);
}

bool CompressorZLIB::Process(const uint8_t* input_data,
                             int input_size,
                             uint8_t* output_data,
                             int output_size,
                             CompressorFlush flush,
                             int* consumed,
                             int* written)
{
    DCHECK_GT(output_size, 0);

    // Setup I/O parameters.
    stream_.avail_in  = input_size;
    stream_.next_in   = input_data;
    stream_.avail_out = output_size;
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
            DLOG(ERROR) << "Unsupported flush mode";
            break;
    }

    int ret = deflate(&stream_, z_flush);
    if (ret == Z_STREAM_ERROR)
    {
        DLOG(ERROR) << "zlib compression failed";
    }

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
            DLOG(ERROR) << "Unexpected zlib error: " << ret;
            break;
    }

    return false;
}

} // namespace aspia
