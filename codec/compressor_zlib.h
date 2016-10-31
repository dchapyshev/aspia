/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/compressor_zlib.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__COMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__COMPRESSOR_ZLIB_H

#include <stdint.h>
#include <memory>

#include "3rdparty/zlib-ng/zlib.h"

#include "base/macros.h"
#include "base/logging.h"

#include "codec/compressor.h"

class CompressorZLIB : public Compressor
{
public:
    CompressorZLIB();
    virtual ~CompressorZLIB();

    virtual bool Process(const uint8_t *input_data,
                         int input_size,
                         uint8_t *output_data,
                         int output_size,
                         CompressorFlush flush,
                         int *consumed,
                         int *written) override;

    virtual void Reset() override;

private:
    z_stream stream_;

    DISALLOW_COPY_AND_ASSIGN(CompressorZLIB);
};

#endif // _ASPIA_CODEC__COMPRESSOR_ZLIB_H
