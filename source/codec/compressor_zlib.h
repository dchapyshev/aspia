//
// PROJECT:         Aspia
// FILE:            codec/compressor_zlib.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__COMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__COMPRESSOR_ZLIB_H

#include <zlib.h>

#include "codec/compressor.h"

namespace aspia {

class CompressorZLIB : public Compressor
{
public:
    explicit CompressorZLIB(int compress_ratio);
    ~CompressorZLIB();

    bool process(const quint8* input_data,
                 size_t input_size,
                 quint8* output_data,
                 size_t output_size,
                 CompressorFlush flush,
                 size_t* consumed,
                 size_t* written) override;

    void reset() override;

private:
    z_stream stream_;

    Q_DISABLE_COPY(CompressorZLIB)
};

} // namespace aspia

#endif // _ASPIA_CODEC__COMPRESSOR_ZLIB_H
