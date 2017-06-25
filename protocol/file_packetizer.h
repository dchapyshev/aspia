//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_packetizer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_PACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_PACKETIZER_H

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
    // Parameter |path| contains the full path to the file in the UTF-8 encoding.
    // If the specified file can not be opened for reading, then returns nullptr.
    static std::unique_ptr<FilePacketizer> Create(const std::string& path);

    enum class State { ERROR, PACKET, LAST_PACKET };

    // Creates a packet for transferring.
    // If an error occurred while creating the packet (for example, it could not
    // read the data from the file), then |State::ERROR| is returned.
    // If the file is read completely and there are no further packets, then 
    // |State::LAST_PACKET| is returned.
    // If the file is not completely read, then |State::PACKET| is returned.
    // Parameter |packet| is valid if the return value is not |State::ERROR|.
    State CreateNextPacket(std::unique_ptr<proto::FilePacket>& packet);

private:
    FilePacketizer(std::wstring&& file_path, std::wifstream&& file_stream);

    uint8_t* GetOutputBuffer(proto::FilePacket* packet, size_t size);

    std::wstring file_path_;
    std::wifstream file_stream_;

    std::streamoff file_size_ = 0;
    std::streamoff left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FilePacketizer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_PACKETIZER_H
