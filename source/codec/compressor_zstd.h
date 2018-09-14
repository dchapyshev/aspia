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

#ifndef ASPIA_CODEC__COMPRESSOR_ZSTD_H_
#define ASPIA_CODEC__COMPRESSOR_ZSTD_H_

#include <zstd.h>

#include "base/macros_magic.h"
#include "codec/compressor.h"

namespace aspia {

class CompressorZstd : public Compressor
{
public:
    explicit CompressorZstd(int compress_ratio);
    ~CompressorZstd();

    // Compressor implementation.
    bool process(const uint8_t* input_data,
                 size_t input_size,
                 uint8_t* output_data,
                 size_t output_size,
                 CompressorFlush flush,
                 size_t* consumed,
                 size_t* written) override;
    void reset() override;

private:
    const int compress_ratio_;
    ZSTD_CStream* stream_;

    DISALLOW_COPY_AND_ASSIGN(CompressorZstd);
};

} // namespace aspia

#endif // ASPIA_CODEC__COMPRESSOR_ZSTD_H_
