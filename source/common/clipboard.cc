//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/clipboard.h"

#include "base/logging.h"

#include <zstd.h>

namespace common {

namespace {

uint8_t* outputBuffer(std::string* out, size_t size)
{
    out->resize(size);
    return reinterpret_cast<uint8_t*>(out->data());
}

} // namespace

const char Clipboard::kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";
const char Clipboard::kMimeTypeCompressedTextUtf8[] = "text/plain; charset=UTF-8; compression=ZSTD";
const int Clipboard::kCompressionRatio = 8;
const size_t Clipboard::kMinSizeToCompress = 512;

// static
bool Clipboard::compress(const std::string& in, std::string* out)
{
    if (in.empty())
        return false;

    const size_t input_size = in.size();
    const uint8_t* input_data = reinterpret_cast<const uint8_t*>(in.data());

    size_t output_size = ZSTD_compressBound(in.size());
    uint8_t* output_data = outputBuffer(out, output_size);

    size_t ret = ZSTD_compress(
        output_data, output_size, input_data, input_size, kCompressionRatio);
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_compress failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    out->resize(ret);
    return true;
}

// static
bool Clipboard::decompress(const std::string& in, std::string* out)
{
    if (in.empty())
        return false;

    int64_t output_size = ZSTD_getFrameContentSize(in.data(), in.size());
    if (output_size == ZSTD_CONTENTSIZE_ERROR)
    {
        LOG(LS_ERROR) << "Data not compressed by ZSTD";
        return false;
    }

    if (output_size == ZSTD_CONTENTSIZE_UNKNOWN)
    {
        LOG(LS_ERROR) << "Original size unknown";
        return false;
    }

    if (!output_size)
        return false;

    uint8_t* output_data = outputBuffer(out, static_cast<size_t>(output_size));

    size_t ret = ZSTD_decompress(
        output_data, static_cast<size_t>(output_size), in.data(), in.size());
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_decompress failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    return true;
}

} // namespace common
