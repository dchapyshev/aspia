//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
FileDepacketizer::FileDepacketizer(const QString& file_path, std::unique_ptr<QFile> file)
    : file_path_(file_path),
      file_(std::move(file))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FileDepacketizer::~FileDepacketizer()
{
    // If the file is opened, it was not completely written.
    if (file_->isOpen())
    {
        file_->close();

        // The transfer of files was canceled. Delete the file.
        QFile::remove(file_path_);
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FileDepacketizer> FileDepacketizer::create(const QString& file_path, bool overwrite)
{
    QFile::OpenMode mode = QFile::WriteOnly;

    if (overwrite)
        mode |= QFile::Truncate;

    std::unique_ptr<QFile> file = std::make_unique<QFile>(file_path);
    if (!file->open(mode))
        return nullptr;

    return std::unique_ptr<FileDepacketizer>(new FileDepacketizer(file_path, std::move(file)));
}

//--------------------------------------------------------------------------------------------------
bool FileDepacketizer::writeNextPacket(const proto::FilePacket& packet)
{
    DCHECK(file_->isOpen());

    const size_t packet_size = packet.data().size();
    if (!packet_size)
    {
        if (packet.flags() & proto::FilePacket::LAST_PACKET)
        {
            if (packet.flags() & proto::FilePacket::FIRST_PACKET)
            {
                // Zero-length file received.
                file_size_ = 0;
                file_->close();
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

    if (!file_->seek(file_size_ - left_size_))
    {
        LOG(LS_ERROR) << "seek failed";
        return false;
    }

    if (file_->write(packet.data().data(), packet_size) == -1)
    {
        LOG(LS_ERROR) << "Unable to write file";
        return false;
    }

    left_size_ -= packet_size;

    if (packet.flags() & proto::FilePacket::LAST_PACKET)
    {
        file_size_ = 0;
        file_->close();
    }

    return true;
}

} // namespace common
