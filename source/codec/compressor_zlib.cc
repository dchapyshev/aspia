//
// PROJECT:         Aspia
// FILE:            codec/compressor_zlib.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/compressor_zlib.h"

#include <QDebug>

namespace aspia {

CompressorZLIB::CompressorZLIB(int compress_ratio)
{
    memset(&stream_, 0, sizeof(stream_));

    int ret = deflateInit2(&stream_,
                           compress_ratio,
                           Z_DEFLATED,
                           MAX_WBITS,
                           MAX_MEM_LEVEL,
                           Z_DEFAULT_STRATEGY);
    Q_ASSERT(ret == Z_OK);
}

CompressorZLIB::~CompressorZLIB()
{
    int ret = deflateEnd(&stream_);
    Q_ASSERT(ret == Z_OK);
}

void CompressorZLIB::reset()
{
    int ret = deflateReset(&stream_);
    Q_ASSERT(ret == Z_OK);
}

bool CompressorZLIB::process(const quint8* input_data,
                             size_t input_size,
                             quint8* output_data,
                             size_t output_size,
                             CompressorFlush flush,
                             size_t* consumed,
                             size_t* written)
{
    Q_ASSERT(output_size != 0);

    // Setup I/O parameters.
    stream_.avail_in  = static_cast<quint32>(input_size);
    stream_.next_in   = input_data;
    stream_.avail_out = static_cast<quint32>(output_size);
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
            qWarning("Unsupported flush mode");
            break;
    }

    int ret = deflate(&stream_, z_flush);
    if (ret == Z_STREAM_ERROR)
        qWarning("zlib compression failed");

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
            qWarning() << "Unexpected zlib error: " << ret;
            break;
    }

    return false;
}

} // namespace aspia
