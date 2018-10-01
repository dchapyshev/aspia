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

#include "common/file_packetizer.h"

#include "base/logging.h"
#include "common/file_packet.h"

namespace aspia {

namespace {

// Files with a size less than or equal to this value will not be compressed.
constexpr size_t kUncompressedFileSize = 1 * 1024; // 1 kB

// The value can be in the range from 1 to 22.
constexpr int kCompressLevel = 6;

char* outputBuffer(proto::file_transfer::Packet* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return packet->mutable_data()->data();
}

} // namespace

FilePacketizer::FilePacketizer(std::ifstream&& file_stream)
    : file_stream_(std::move(file_stream))
{
    read_buffer_.resize(kMaxFilePacketSize);

    file_stream_.seekg(0, file_stream_.end);
    file_size_ = file_stream_.tellg();
    file_stream_.seekg(0);
    left_size_ = file_size_;

    if (file_size_ > kUncompressedFileSize)
    {
        compressor_.reset(ZSTD_createCStream());
        DCHECK(compressor_);
    }
}

std::unique_ptr<FilePacketizer> FilePacketizer::create(const std::filesystem::path& file_path)
{
    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
        return nullptr;

    return std::unique_ptr<FilePacketizer>(new FilePacketizer(std::move(file_stream)));
}

std::unique_ptr<proto::file_transfer::Packet> FilePacketizer::readNextPacket(
    const proto::file_transfer::PacketRequest& request)
{
    DCHECK(file_stream_.is_open());

    // Create a new file packet.
    std::unique_ptr<proto::file_transfer::Packet> packet =
        std::make_unique<proto::file_transfer::Packet>();

    // If a command is received to complete the transfer of the current file.
    if (request.flags() & proto::file_transfer::PacketRequest::CANCEL)
    {
        // Set the flag of the last packet and exit.
        packet->set_flags(proto::file_transfer::Packet::LAST_PACKET);
        return packet;
    }

    if (left_size_ == file_size_)
    {
        uint32_t flags = proto::file_transfer::Packet::FIRST_PACKET;

        if (compressor_)
            flags |= proto::file_transfer::Packet::COMPRESSED;

        // Set file path and size in first packet.
        packet->set_file_size(file_size_);
        packet->set_flags(flags);
    }

    const uint64_t read_size = std::min(static_cast<uint64_t>(read_buffer_.size()), left_size_);

    // Moving to a new position in file.
    file_stream_.seekg(file_size_ - left_size_);

    // Read next part of file.
    file_stream_.read(read_buffer_.data(), read_size);
    if (file_stream_.fail())
    {
        LOG(LS_WARNING) << "Unable to read file";
        return nullptr;
    }

    left_size_ -= read_size;

    if (compressor_)
    {
        // Initialize the compression stream.
        size_t ret = ZSTD_initCStream(compressor_.get(), kCompressLevel);
        DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

        // Prepare the buffer for compression.
        const size_t output_size = ZSTD_compressBound(static_cast<size_t>(read_size));
        char* output_data = outputBuffer(packet.get(), output_size);

        ZSTD_inBuffer input = { read_buffer_.data(), read_buffer_.size(), 0 };
        ZSTD_outBuffer output = { output_data, output_size, 0 };

        while (input.pos < input.size)
        {
            ret = ZSTD_compressStream(compressor_.get(), &output, &input);
            if (ZSTD_isError(ret))
            {
                LOG(LS_WARNING) << "ZSTD_compressStream failed: " << ZSTD_getErrorName(ret);
                return false;
            }
        }

        ret = ZSTD_endStream(compressor_.get(), &output);
        DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

        packet->mutable_data()->resize(output.pos);
    }
    else
    {
        packet->set_data(read_buffer_);
    }

    if (!left_size_)
    {
        file_size_ = 0;
        file_stream_.close();

        packet->set_flags(packet->flags() | proto::file_transfer::Packet::LAST_PACKET);
    }

    return packet;
}

} // namespace aspia
