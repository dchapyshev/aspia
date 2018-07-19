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

#ifndef ASPIA_CODEC__DECOMPRESSOR_H_
#define ASPIA_CODEC__DECOMPRESSOR_H_

#include <qglobal.h>

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
    virtual bool process(const quint8* input_data,
                         size_t input_size,
                         quint8* output_data,
                         size_t output_size,
                         size_t* consumed,
                         size_t* written) = 0;
};

} // namespace aspia

#endif // ASPIA_CODEC__DECOMPRESSOR_H_
