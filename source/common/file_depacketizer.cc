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

#include "common/file_depacketizer.h"

#include "base/logging.h"

namespace aspia {

namespace {

// When transferring a file is divided into parts and each part is transmitted separately.
// This parameter specifies the size of the part.
constexpr size_t kPacketPartSize = 32 * 1024; // 32 kB

} // namespace

FileDepacketizer::FileDepacketizer(const std::filesystem::path& file_path,
                                   std::ofstream&& file_stream)
    : file_path_(file_path),
      file_stream_(std::move(file_stream))
{
    // Nothing
}

FileDepacketizer::~FileDepacketizer()
{
    // If the file is opened, it was not completely written.
    if (file_stream_.is_open())
    {
        file_stream_.close();

        // The transfer of files was canceled. Delete the file.
        std::error_code ignored_error;
        std::filesystem::remove(file_path_, ignored_error);
    }
}

// static
std::unique_ptr<FileDepacketizer> FileDepacketizer::create(
    const std::filesystem::path& file_path, bool overwrite)
{
    std::ofstream::openmode mode = std::ofstream::binary;

    if (overwrite)
        mode |= std::ofstream::trunc;

    std::ofstream file_stream;

    file_stream.open(file_path, mode);
    if (!file_stream.is_open())
        return nullptr;

    return std::unique_ptr<FileDepacketizer>(
        new FileDepacketizer(file_path, std::move(file_stream)));
}

bool FileDepacketizer::writeNextPacket(const proto::file_transfer::Packet& packet)
{
    DCHECK(file_stream_.is_open());

    if (packet.data().empty())
    {
        // If an empty data packet with the last packet flag set is received, the transfer
        // is canceled.
        if (packet.flags() & proto::file_transfer::Packet::LAST_PACKET)
            return true;

        LOG(LS_WARNING) << "Wrong packet size";
        return false;
    }

    // The first packet must have the full file size.
    if (packet.flags() & proto::file_transfer::Packet::FIRST_PACKET)
    {
        file_size_ = packet.file_size();
        left_size_ = file_size_;

        if (packet.flags() & proto::file_transfer::Packet::COMPRESSED)
        {
            decompressor_.reset(ZSTD_createDStream());
            if (!decompressor_)
            {
                LOG(LS_WARNING) << "ZSTD_createDStream failed";
                return false;
            }

            write_buffer_.resize(kPacketPartSize);
        }
    }

    const char* write_data;
    size_t write_size;

    if (decompressor_)
    {
        size_t ret = ZSTD_initDStream(decompressor_.get());
        DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

        ZSTD_inBuffer input = { packet.data().data(), packet.data().size(), 0 };
        ZSTD_outBuffer output = { write_buffer_.data(), write_buffer_.size(), 0 };

        while (input.pos < input.size)
        {
            ret = ZSTD_decompressStream(decompressor_.get(), &output, &input);
            if (ZSTD_isError(ret))
            {
                LOG(LS_WARNING) << "ZSTD_decompressStream failed: " << ZSTD_getErrorName(ret);
                return false;
            }
        }

        write_data = write_buffer_.data();
        write_size = output.pos;
    }
    else
    {
        write_data = packet.data().data();
        write_size = packet.data().size();
    }

    file_stream_.seekp(file_size_ - left_size_);
    file_stream_.write(write_data, write_size);
    if (file_stream_.fail())
    {
        LOG(LS_WARNING) << "Unable to write file";
        return false;
    }

    left_size_ -= write_size;

    if (packet.flags() & proto::file_transfer::Packet::LAST_PACKET)
    {
        file_size_ = 0;
        file_stream_.close();
    }

    return true;
}

} // namespace aspia
