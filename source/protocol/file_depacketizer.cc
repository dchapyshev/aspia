//
// PROJECT:         Aspia
// FILE:            protocol/file_depacketizer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/file_depacketizer.h"
#include "base/logging.h"

namespace aspia {

FileDepacketizer::FileDepacketizer(std::ofstream&& file_stream)
    : file_stream_(std::move(file_stream))
{
    // Nothing
}

// static
std::unique_ptr<FileDepacketizer> FileDepacketizer::Create(
    const std::experimental::filesystem::path& file_path, bool overwrite)
{
    std::ofstream::openmode mode = std::ofstream::binary;

    if (overwrite)
        mode |= std::ofstream::trunc;

    std::ofstream file_stream;

    file_stream.open(file_path, mode);
    if (!file_stream.is_open())
    {
        LOG(LS_ERROR) << "Unable to create file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<FileDepacketizer>(new FileDepacketizer(std::move(file_stream)));
}

bool FileDepacketizer::ReadNextPacket(const proto::file_transfer::FilePacket& packet)
{
    DCHECK(file_stream_.is_open());

    // The first packet must have the full file size.
    if (packet.flags() & proto::file_transfer::FilePacket::FLAG_FIRST_PACKET)
    {
        file_size_ = packet.file_size();
        left_size_ = packet.file_size();
    }

    const size_t packet_size = packet.data().size();

    file_stream_.seekp(file_size_ - left_size_);
    file_stream_.write(packet.data().data(), packet_size);
    if (file_stream_.fail())
    {
        DLOG(LS_WARNING) << "Unable to write file";
        return false;
    }

    left_size_ -= packet_size;

    if (packet.flags() & proto::file_transfer::FilePacket::FLAG_LAST_PACKET)
    {
        file_size_ = 0;
        file_stream_.close();
    }

    return true;
}

} // namespace aspia
