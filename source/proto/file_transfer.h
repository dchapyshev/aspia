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

#ifndef PROTO_FILE_TRANSFER_H
#define PROTO_FILE_TRANSFER_H

#include <QMetaType>

#include "proto/file_transfer.pb.h"

Q_DECLARE_METATYPE(proto::DriveList)
Q_DECLARE_METATYPE(proto::DriveList::Item)
Q_DECLARE_METATYPE(proto::DriveList::Item::Type)
Q_DECLARE_METATYPE(proto::DriveListRequest)
Q_DECLARE_METATYPE(proto::FileList)
Q_DECLARE_METATYPE(proto::FileList::Item)
Q_DECLARE_METATYPE(proto::FileListRequest)
Q_DECLARE_METATYPE(proto::UploadRequest)
Q_DECLARE_METATYPE(proto::DownloadRequest)
Q_DECLARE_METATYPE(proto::FilePacketRequest)
Q_DECLARE_METATYPE(proto::FilePacket)
Q_DECLARE_METATYPE(proto::CreateDirectoryRequest)
Q_DECLARE_METATYPE(proto::RenameRequest)
Q_DECLARE_METATYPE(proto::RemoveRequest)
Q_DECLARE_METATYPE(proto::FileError)
Q_DECLARE_METATYPE(proto::FileReply)
Q_DECLARE_METATYPE(proto::FileRequest)

#endif // PROTO_FILE_TRANSFER_H
