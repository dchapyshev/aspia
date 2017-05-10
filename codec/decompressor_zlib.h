//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/decompressor_zlib.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H

#include "base/macros.h"
#include "3rdparty/zlib-ng/zlib.h"
#include "codec/decompressor.h"

namespace aspia {

class DecompressorZLIB : public Decompressor
{
public:
    DecompressorZLIB();
    ~DecompressorZLIB();

    void Reset() override;

    // Decompressor implementations.
    bool Process(const uint8_t* input_data,
                 int input_size,
                 uint8_t* output_data,
                 int output_size,
                 int* consumed,
                 int* written) override;

private:
    z_stream stream_;

    DISALLOW_COPY_AND_ASSIGN(DecompressorZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
