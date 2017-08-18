//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_depacketizer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <fstream>
#include <memory>

namespace aspia {

class FileDepacketizer
{
public:
    ~FileDepacketizer() = default;

    static std::unique_ptr<FileDepacketizer> Create(const FilePath& file_path, bool overwrite);

    // Reads the packet and writes its contents to a file.
    bool ReadNextPacket(const proto::FilePacket& packet);

    uint64_t LeftSize() const { return left_size_; }

private:
    FileDepacketizer(std::ofstream&& file_stream);

    std::ofstream file_stream_;

    std::streamoff file_size_ = 0;
    std::streamoff left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileDepacketizer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
