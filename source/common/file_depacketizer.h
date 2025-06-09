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

#ifndef COMMON_FILE_DEPACKETIZER_H
#define COMMON_FILE_DEPACKETIZER_H

#include "base/macros_magic.h"
#include "proto/file_transfer.h"

#include <memory>

#include <QFile>

namespace common {

class FileDepacketizer
{
public:
    ~FileDepacketizer();

    static std::unique_ptr<FileDepacketizer> create(const QString& file_path, bool overwrite);

    // Reads the packet and writes its contents to a file.
    bool writeNextPacket(const proto::file_transfer::Packet& packet);

private:
    FileDepacketizer(const QString& file_path, std::unique_ptr<QFile> file);

    QString file_path_;
    std::unique_ptr<QFile> file_;

    quint64 file_size_ = 0;
    quint64 left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileDepacketizer);
};

} // namespace common

#endif // COMMON_FILE_DEPACKETIZER_H
