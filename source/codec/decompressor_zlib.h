//
// PROJECT:         Aspia
// FILE:            codec/decompressor_zlib.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H

#include <zlib.h>

#include "codec/decompressor.h"

namespace aspia {

class DecompressorZLIB : public Decompressor
{
public:
    DecompressorZLIB();
    ~DecompressorZLIB();

    void reset() override;

    // Decompressor implementations.
    bool process(const quint8* input_data,
                 size_t input_size,
                 quint8* output_data,
                 size_t output_size,
                 size_t* consumed,
                 size_t* written) override;

private:
    z_stream stream_;

    Q_DISABLE_COPY(DecompressorZLIB)
};

} // namespace aspia

#endif // _ASPIA_CODEC__DECOMPRESSOR_ZLIB_H
