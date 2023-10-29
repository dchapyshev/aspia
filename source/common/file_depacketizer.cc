//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/file_depacketizer.h"

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileDepacketizer::FileDepacketizer(const std::filesystem::path& file_path,
                                   std::ofstream&& file_stream)
    : file_path_(file_path),
      file_stream_(std::move(file_stream))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FileDepacketizer::~FileDepacketizer()
{
    // If the file is opened, it was not completely written.
    if (file_stream_.is_open())
    {
        file_stream_.close();

        // The transfer of files was canceled. Delete the file.
        std::error_code ignored_error;
        std::filesystem::remove(file_path_, ignored_error);
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FileDepacketizer> FileDepacketizer::create(
    const std::filesystem::path& file_path, bool overwrite)
{
    std::ofstream::openmode mode = std::ofstream::binary;

    if (overwrite)
        mode |= std::ofstream::trunc;

    std::ofstream file_stream;

    file_stream.open(file_path, mode);
    if (!file_stream.is_open())
        return nullptr;

    return std::unique_ptr<FileDepacketizer>(
        new FileDepacketizer(file_path, std::move(file_stream)));
}

//--------------------------------------------------------------------------------------------------
bool FileDepacketizer::writeNextPacket(const proto::FilePacket& packet)
{
    DCHECK(file_stream_.is_open());

    const size_t packet_size = packet.data().size();
    if (!packet_size)
    {
        if (packet.flags() & proto::FilePacket::LAST_PACKET)
        {
            if (packet.flags() & proto::FilePacket::FIRST_PACKET)
            {
                // Zero-length file received.
                file_size_ = 0;
                file_stream_.close();
            }
            else
            {
                // If an empty data packet with the last packet flag set is received, the transfer
                // is canceled.
            }

            return true;
        }

        LOG(LS_ERROR) << "Wrong packet size";
        return false;
    }

    // The first packet must have the full file size.
    if (packet.flags() & proto::FilePacket::FIRST_PACKET)
    {
        file_size_ = packet.file_size();
        left_size_ = file_size_;
    }

    file_stream_.seekp(static_cast<std::streamoff>(file_size_ - left_size_));
    file_stream_.write(packet.data().data(), packet_size);
    if (file_stream_.fail())
    {
        LOG(LS_ERROR) << "Unable to write file";
        return false;
    }

    left_size_ -= packet_size;

    if (packet.flags() & proto::FilePacket::LAST_PACKET)
    {
        file_size_ = 0;
        file_stream_.close();
    }

    return true;
}

} // namespace common
