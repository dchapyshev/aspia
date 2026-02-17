//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/zstd_compress.h"

#include <QtEndian>

#include "base/logging.h"
#include "base/codec/scoped_zstd_stream.h"

namespace base {

namespace {

const quint32 kMaxDataSize = 64 * 1024 * 1024; // 64 MB

//--------------------------------------------------------------------------------------------------
template <class T>
T compressT(const T& source, int compress_level)
{
    quint32 source_data_size = static_cast<quint32>(source.size());
    if (!source_data_size || source_data_size > kMaxDataSize)
    {
        LOG(ERROR) << "Invalid source data size:" << source_data_size;
        return T();
    }

    ScopedZstdCStream stream;
    size_t ret = ZSTD_initCStream(stream.get(), compress_level);
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_initCStream failed:" << ZSTD_getErrorName(ret) << "(" << ret << ")";
        return T();
    }

    const size_t input_size = source.size();
    const quint8* input_data = reinterpret_cast<const quint8*>(source.data());

    const size_t output_size = ZSTD_compressBound(input_size);

    T target;
    target.resize(static_cast<T::size_type>(output_size + sizeof(quint32)));

    source_data_size = qToBigEndian(source_data_size);
    memcpy(target.data(), &source_data_size, sizeof(quint32));

    quint8* output_data = reinterpret_cast<quint8*>(target.data() + sizeof(quint32));

    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_compressStream(stream.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(ERROR) << "ZSTD_compressStream failed:" << ZSTD_getErrorName(ret) << "(" << ret << ")";
            return T();
        }
    }

    ret = ZSTD_endStream(stream.get(), &output);
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_endStream failed:" << ZSTD_getErrorName(ret) << "(" << ret << ")";
        return T();
    }

    return target;
}

//--------------------------------------------------------------------------------------------------
template <class T>
T decompressT(const T& source)
{
    if (source.size() < sizeof(quint32) + 1)
        return T();

    quint32 target_data_size;

    memcpy(&target_data_size, source.data(), sizeof(quint32));
    target_data_size = qFromBigEndian(target_data_size);

    if (!target_data_size || target_data_size > kMaxDataSize)
    {
        LOG(ERROR) << "Invalid target data size:" << target_data_size;
        return T();
    }

    T target;
    target.resize(target_data_size);

    ScopedZstdDStream stream;
    size_t ret = ZSTD_initDStream(stream.get());
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_initDStream failed:" << ZSTD_getErrorName(ret) << "(" << ret << ")";
        return T();
    }

    ZSTD_inBuffer input = { source.data() + sizeof(quint32), source.size() - sizeof(quint32), 0 };
    ZSTD_outBuffer output = { target.data(), static_cast<size_t>(target.size()), 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_decompressStream(stream.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(ERROR) << "ZSTD_decompressStream failed:" << ZSTD_getErrorName(ret)
                       << "(" << ret << ")";
            return T();
        }
    }

    return target;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::string ZstdCompress::compress(const std::string& source, int compress_level)
{
    return compressT(source, compress_level);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray ZstdCompress::compress(const QByteArray& source, int compress_level)
{
    return compressT(source, compress_level);
}

//--------------------------------------------------------------------------------------------------
// static
std::string ZstdCompress::decompress(const std::string& source)
{
    return decompressT(source);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray ZstdCompress::decompress(const QByteArray& source)
{
    return decompressT(source);
}

} // namespace base
