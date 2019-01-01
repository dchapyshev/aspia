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

char* outputBuffer(proto::file_transfer::Packet* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return packet->mutable_data()->data();
}

} // namespace

FilePacketizer::FilePacketizer(std::ifstream&& file_stream)
    : file_stream_(std::move(file_stream))
{
    file_stream_.seekg(0, file_stream_.end);
    file_size_ = file_stream_.tellg();
    file_stream_.seekg(0);
    left_size_ = file_size_;
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

    if (request.flags() & proto::file_transfer::PacketRequest::CANCEL)
    {
        packet->set_flags(proto::file_transfer::Packet::LAST_PACKET);
        return packet;
    }

    size_t packet_buffer_size = kMaxFilePacketSize;

    if (left_size_ < kMaxFilePacketSize)
        packet_buffer_size = static_cast<size_t>(left_size_);

    char* packet_buffer = outputBuffer(packet.get(), packet_buffer_size);

    // Moving to a new position in file.
    file_stream_.seekg(file_size_ - left_size_);

    file_stream_.read(packet_buffer, packet_buffer_size);
    if (file_stream_.fail())
    {
        LOG(LS_WARNING) << "Unable to read file";
        return nullptr;
    }

    if (left_size_ == file_size_)
    {
        packet->set_flags(packet->flags() | proto::file_transfer::Packet::FIRST_PACKET);

        // Set file path and size in first packet.
        packet->set_file_size(file_size_);
    }

    left_size_ -= packet_buffer_size;

    if (!left_size_)
    {
        file_size_ = 0;
        file_stream_.close();

        packet->set_flags(packet->flags() | proto::file_transfer::Packet::LAST_PACKET);
    }

    return packet;
}

} // namespace aspia
