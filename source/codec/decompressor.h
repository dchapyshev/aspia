//
// PROJECT:         Aspia
// FILE:            codec/decompressor.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__DECOMPRESSOR_H
#define _ASPIA_CODEC__DECOMPRESSOR_H

#include <cstdint>

namespace aspia {

//
// An object to decompress data losslessly. This is used in pair with
// a compressor.
//
// Note that a decompressor can only be used on one stream during its
// lifetime. This object should be destroyed after use.
//

class Decompressor
{
public:
    virtual ~Decompressor() = default;

    //
    // Resets all the internal state so the decompressor behaves as if it was
    // just created.
    //
    virtual void reset() = 0;

    //
    // Decompress |input_data| with |input_size| bytes.
    //
    // |output_data| is provided by the caller and |output_size| is the
    // size of |output_data|. |output_size| must be greater than 0.
    //
    // Decompressed data is written to |output_data|. |consumed| will
    // contain the number of bytes consumed from the input. |written|
    // contains the number of bytes written to output.
    //
    // Returns true if this method needs to be called again because
    // there is more bytes to be decompressed or more input data is
    // needed.
    //
    virtual bool process(const uint8_t* input_data,
                         size_t input_size,
                         uint8_t* output_data,
                         size_t output_size,
                         size_t* consumed,
                         size_t* written) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__DECOMPRESSOR_H
