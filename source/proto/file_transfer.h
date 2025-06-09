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

Q_DECLARE_METATYPE(proto::file_transfer::DriveList)
Q_DECLARE_METATYPE(proto::file_transfer::DriveList::Item)
Q_DECLARE_METATYPE(proto::file_transfer::DriveList::Item::Type)
Q_DECLARE_METATYPE(proto::file_transfer::DriveListRequest)
Q_DECLARE_METATYPE(proto::file_transfer::List)
Q_DECLARE_METATYPE(proto::file_transfer::List::Item)
Q_DECLARE_METATYPE(proto::file_transfer::ListRequest)
Q_DECLARE_METATYPE(proto::file_transfer::UploadRequest)
Q_DECLARE_METATYPE(proto::file_transfer::DownloadRequest)
Q_DECLARE_METATYPE(proto::file_transfer::PacketRequest)
Q_DECLARE_METATYPE(proto::file_transfer::Packet)
Q_DECLARE_METATYPE(proto::file_transfer::CreateDirectoryRequest)
Q_DECLARE_METATYPE(proto::file_transfer::RenameRequest)
Q_DECLARE_METATYPE(proto::file_transfer::RemoveRequest)
Q_DECLARE_METATYPE(proto::file_transfer::ErrorCode)
Q_DECLARE_METATYPE(proto::file_transfer::Reply)
Q_DECLARE_METATYPE(proto::file_transfer::Request)

#endif // PROTO_FILE_TRANSFER_H
