//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_CODEC__COMPRESSOR_H_
#define ASPIA_CODEC__COMPRESSOR_H_

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
    virtual void reset() = 0;

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
    virtual bool process(const uint8_t* input_data,
                         size_t input_size,
                         uint8_t* output_data,
                         size_t output_size,
                         CompressorFlush flush,
                         size_t* consumed,
                         size_t* written) = 0;
};

} // namespace aspia

#endif // ASPIA_CODEC__COMPRESSOR_H_
