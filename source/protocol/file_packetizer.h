//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_packetizer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_PACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_PACKETIZER_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <fstream>
#include <memory>

namespace aspia {

class FilePacketizer
{
public:
    ~FilePacketizer() = default;

    // Creates an instance of the class.
    // Parameter |file_path| contains the full path to the file.
    // If the specified file can not be opened for reading, then returns nullptr.
    static std::unique_ptr<FilePacketizer> Create(const FilePath& file_path);

    // Creates a packet for transferring.
    std::unique_ptr<proto::FilePacket> CreateNextPacket();

    uint64_t LeftSize() const { return left_size_; }

private:
    FilePacketizer(std::ifstream&& file_stream);

    std::ifstream file_stream_;

    std::streamoff file_size_ = 0;
    std::streamoff left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FilePacketizer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_PACKETIZER_H
