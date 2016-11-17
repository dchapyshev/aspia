/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/decompressor_zlib.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H

#include <stdint.h>

#include "base/macros.h"
#include "3rdparty/zlib-ng/zlib.h"
#include "codec/decompressor.h"

class DecompressorZLIB : public Decompressor
{
public:
    DecompressorZLIB();
    virtual ~DecompressorZLIB();

    virtual void Reset() override;

    // Decompressor implementations.
    virtual bool Process(const uint8_t *input_data,
                         int input_size,
                         uint8_t *output_data,
                         int output_size,
                         int *consumed,
                         int *written) override;

private:
    void InitStream();
    z_stream stream_;

    DISALLOW_COPY_AND_ASSIGN(DecompressorZLIB);
};

#endif // _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
