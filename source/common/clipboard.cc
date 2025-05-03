//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";
const char kMimeTypeCompressedTextUtf8[] = "text/plain; charset=UTF-8; compression=ZSTD";

// The compression ratio can be in the range of 1 to 22.
const int kCompressionRatio = 8;

// Smaller data will not be compressed.
const size_t kMinSizeToCompress = 512;

//--------------------------------------------------------------------------------------------------
quint8* outputBuffer(std::string* out, size_t size)
{
    out->resize(size);
    return reinterpret_cast<quint8*>(out->data());
}

//--------------------------------------------------------------------------------------------------
bool compress(const std::string& in, std::string* out)
{
    if (in.empty())
        return false;

    const size_t input_size = in.size();
    const quint8* input_data = reinterpret_cast<const quint8*>(in.data());

    size_t output_size = ZSTD_compressBound(in.size());
    quint8* output_data = outputBuffer(out, output_size);

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

//--------------------------------------------------------------------------------------------------
bool decompress(const std::string& in, std::string* out)
{
    if (in.empty())
        return false;

    unsigned long long output_size = ZSTD_getFrameContentSize(in.data(), in.size());
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

    quint8* output_data = outputBuffer(out, static_cast<size_t>(output_size));

    size_t ret = ZSTD_decompress(
        output_data, static_cast<size_t>(output_size), in.data(), in.size());
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_decompress failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Clipboard::start()
{
    init();
}

//--------------------------------------------------------------------------------------------------
void Clipboard::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (event.mime_type() == kMimeTypeCompressedTextUtf8)
    {
        std::string decompressed_data;
        if (!decompress(event.data(), &decompressed_data))
            return;

        // Store last injected data.
        last_data_ = std::move(decompressed_data);
    }
    else if (event.mime_type() == kMimeTypeTextUtf8)
    {
        // Store last injected data.
        last_data_ = event.data();
    }
    else
    {
        LOG(LS_ERROR) << "Unsupported mime type: " << event.mime_type();
        return;
    }

    setData(last_data_);
}

//--------------------------------------------------------------------------------------------------
void Clipboard::clearClipboard()
{
    setData(std::string());
}

//--------------------------------------------------------------------------------------------------
void Clipboard::onData(const std::string& data)
{
    if (last_data_ == data)
        return;

    proto::ClipboardEvent event;

    if (data.size() > kMinSizeToCompress)
    {
        std::string compressed_data;
        if (!compress(data, &compressed_data))
            return;

        event.set_mime_type(kMimeTypeCompressedTextUtf8);
        event.set_data(std::move(compressed_data));
    }
    else
    {
        event.set_mime_type(kMimeTypeTextUtf8);
        event.set_data(data);
    }

    emit sig_clipboardEvent(event);
}

} // namespace common
