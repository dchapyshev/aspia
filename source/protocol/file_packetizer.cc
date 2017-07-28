//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_packetizer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/file_packetizer.h"
#include "base/logging.h"

namespace aspia {

// When transferring a file is divided into parts and each part is
// transmitted separately.
// This parameter specifies the size of the part.
static const size_t kPacketPartSize = 10 * 1024; // 10 kB

FilePacketizer::FilePacketizer(std::ifstream&& file_stream)
    : file_stream_(std::move(file_stream))
{
    file_stream_.seekg(0, file_stream_.end);
    file_size_ = static_cast<size_t>(file_stream_.tellg());
    file_stream_.seekg(0);

    left_size_ = file_size_;
}

std::unique_ptr<FilePacketizer> FilePacketizer::Create(
    const FilePath& file_path)
{
    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "Unable to open file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<FilePacketizer>(
        new FilePacketizer(std::move(file_stream)));
}

static char* GetOutputBuffer(proto::FilePacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return const_cast<char*>(packet->mutable_data()->data());
}

std::unique_ptr<proto::FilePacket> FilePacketizer::CreateNextPacket()
{
    DCHECK(file_stream_.is_open());

    if (!file_size_ || !left_size_)
        return nullptr;

    // Create a new file packet.
    std::unique_ptr<proto::FilePacket> packet =
        std::make_unique<proto::FilePacket>();

    // All file packets must have the flag.
    packet->set_flags(proto::FilePacket::PACKET);

    size_t packet_buffer_size = kPacketPartSize;

    if (left_size_ < kPacketPartSize)
        packet_buffer_size = static_cast<size_t>(left_size_);

    char* packet_buffer = GetOutputBuffer(packet.get(), packet_buffer_size);

    // Moving to a new position in file.
    file_stream_.seekg(file_size_ - left_size_);

    file_stream_.read(packet_buffer, packet_buffer_size);
    if (file_stream_.fail())
    {
        DLOG(ERROR) << "Unable to read file";
        return nullptr;
    }

    if (left_size_ == file_size_)
    {
        packet->set_flags(packet->flags() | proto::FilePacket::FIRST_PACKET);

        // Set file path and size in first packet.
        packet->set_file_size(file_size_);
    }

    left_size_ -= packet_buffer_size;

    if (!left_size_)
    {
        file_size_ = 0;
        file_stream_.close();

        packet->set_flags(packet->flags() | proto::FilePacket::LAST_PACKET);
    }

    return packet;
}

} // namespace aspia
