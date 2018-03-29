//
// PROJECT:         Aspia
// FILE:            protocol/file_packetizer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/file_packetizer.h"

namespace aspia {

namespace {

// When transferring a file is divided into parts and each part is
// transmitted separately.
// This parameter specifies the size of the part.
constexpr qint64 kPacketPartSize = 16 * 1024; // 16 kB

char* GetOutputBuffer(proto::file_transfer::Packet* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return const_cast<char*>(packet->mutable_data()->data());
}

} // namespace

FilePacketizer::FilePacketizer(QPointer<QFile>& file)
{
    file_.swap(file);
    file_size_ = file_->size();
    left_size_ = file_size_;
}

std::unique_ptr<FilePacketizer> FilePacketizer::Create(const QString& file_path)
{
    QPointer<QFile> file = new QFile(file_path);

    if (!file->open(QFile::ReadOnly))
        return nullptr;

    return std::unique_ptr<FilePacketizer>(new FilePacketizer(file));
}

std::unique_ptr<proto::file_transfer::Packet> FilePacketizer::CreateNextPacket()
{
    Q_ASSERT(!file_.isNull() && file_->isOpen());

    // Create a new file packet.
    std::unique_ptr<proto::file_transfer::Packet> packet =
        std::make_unique<proto::file_transfer::Packet>();

    // All file packets must have the flag.
    packet->set_flags(proto::file_transfer::Packet::FLAG_PACKET);

    qint64 packet_buffer_size = kPacketPartSize;

    if (left_size_ < kPacketPartSize)
        packet_buffer_size = static_cast<size_t>(left_size_);

    char* packet_buffer = GetOutputBuffer(packet.get(), packet_buffer_size);

    // Moving to a new position in file.
    file_->seek(file_size_ - left_size_);

    if (file_->read(packet_buffer, packet_buffer_size) != packet_buffer_size)
        return nullptr;

    if (left_size_ == file_size_)
    {
        packet->set_flags(packet->flags() | proto::file_transfer::Packet::FLAG_FIRST_PACKET);

        // Set file path and size in first packet.
        packet->set_file_size(file_size_);
    }

    left_size_ -= packet_buffer_size;

    if (!left_size_)
    {
        file_size_ = 0;
        file_->close();

        packet->set_flags(packet->flags() | proto::file_transfer::Packet::FLAG_LAST_PACKET);
    }

    return packet;
}

} // namespace aspia
