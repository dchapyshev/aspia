//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_depacketizer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace aspia {

class FileDepacketizer
{
public:
    FileDepacketizer() = default;
    ~FileDepacketizer() = default;

    enum class State { ERROR, PACKET, LAST_PACKET };

    // Reads the packet and writes its contents to a file.
    // If an error occurred while reading a package or writing a file,
    // then |State::ERROR| is returned.
    // If the packet read is the last one, then |State::LAST_PACKET| is returned.
    // If the packet is not the last one, then |State::PACKET| is returned.
    State ReadNextPacket(const proto::FilePacket& packet);

private:
    std::experimental::filesystem::path file_path_;
    std::ofstream file_stream_;

    std::streamoff file_size_ = 0;
    std::streamoff left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileDepacketizer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
