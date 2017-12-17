//
// PROJECT:         Aspia
// FILE:            codec/compressor.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__COMPRESSOR_H
#define _ASPIA_CODEC__COMPRESSOR_H

#include <cstdint>

namespace aspia {

//
// An object to compress data losslessly. Compressed data can be fully
// recovered by a Decompressor.
//
// Note that a Compressor can only be used on one stream during its
// lifetime. This object should be destroyed after use.
//

class Compressor
{
public:
    enum CompressorFlush
    {
        CompressorNoFlush,
        CompressorSyncFlush,
        CompressorFinish,
    };

    virtual ~Compressor() = default;

    //
    // Resets all the internal state so the compressor behaves as if it
    // was just created.
    //
    virtual void Reset() = 0;

    //
    // Compress |input_data| with |input_size| bytes.
    //
    // |output_data| is provided by the caller and |output_size| is the
    // size of |output_data|. |output_size| must be greater than 0.
    //
    // |flush| is set to one of the three value:
    // - CompressorNoFlush
    //   No flushing is requested
    // - CompressorSyncFlush
    //   Write all pending output and write a synchronization point in the
    //   output data stream.
    // - CompressorFinish
    //   Mark the end of stream.
    //
    // Compressed data is written to |output_data|. |consumed| will
    // contain the number of bytes consumed from the input. |written|
    // contains the number of bytes written to output.
    //
    // Returns true if this method needs to be called again because
    // there is more data to be written out. This is particularly
    // useful for end of the compression stream.
    //
    virtual bool Process(const uint8_t* input_data,
                         size_t input_size,
                         uint8_t* output_data,
                         size_t output_size,
                         CompressorFlush flush,
                         size_t* consumed,
                         size_t* written) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__COMPRESSOR_H
