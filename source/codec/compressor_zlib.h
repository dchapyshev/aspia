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

#ifndef _ASPIA_CODEC__COMPRESSOR_ZLIB_H
#define _ASPIA_CODEC__COMPRESSOR_ZLIB_H

#include <zlib-ng.h>

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
    zng_stream stream_;

    Q_DISABLE_COPY(CompressorZLIB)
};

} // namespace aspia

#endif // _ASPIA_CODEC__COMPRESSOR_ZLIB_H
