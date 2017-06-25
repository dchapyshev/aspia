//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_depacketizer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/file_depacketizer.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

FileDepacketizer::State FileDepacketizer::ReadNextPacket(
    const proto::FilePacket& packet)
{
    // The first package must have the path and the full file size.
    if (!packet.path().empty() && packet.full_size())
    {
        file_path_ = UNICODEfromUTF8(packet.path());

        file_stream_.open(file_path_, std::ofstream::binary);
        if (!file_stream_.is_open())
        {
            LOG(ERROR) << "Unable to create file: " << file_path_;
            return State::ERROR;
        }

        file_size_ = packet.full_size();
        left_size_ = packet.full_size();
    }

    if (!file_stream_.is_open())
        return State::ERROR;

    const size_t packet_size = packet.data().size();

    file_stream_.seekp(file_size_ - left_size_);
    file_stream_.write(reinterpret_cast<const wchar_t*>(packet.data().data()),
                       packet_size);
    if (file_stream_.fail())
    {
        LOG(WARNING) << "Unable to write file: " << file_path_;
        return State::ERROR;
    }

    left_size_ -= packet_size;

    if (!left_size_)
    {
        file_size_ = 0;
        file_stream_.close();

        return State::LAST_PACKET;
    }

    return State::PACKET;
}

} // namespace aspia
