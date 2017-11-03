//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/compressor_zlib.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__COMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__COMPRESSOR_ZLIB_H

#include "base/macros.h"
#include "codec/compressor.h"

#include <zlib.h>

namespace aspia {

class CompressorZLIB : public Compressor
{
public:
    explicit CompressorZLIB(int32_t compress_ratio);
    ~CompressorZLIB();

    bool Process(const uint8_t* input_data,
                 size_t input_size,
                 uint8_t* output_data,
                 size_t output_size,
                 CompressorFlush flush,
                 size_t* consumed,
                 size_t* written) override;

    void Reset() override;

private:
    z_stream stream_;

    DISALLOW_COPY_AND_ASSIGN(CompressorZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__COMPRESSOR_ZLIB_H
