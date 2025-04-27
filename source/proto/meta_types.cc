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

#include "proto/meta_types.h"

#include "proto/common.h"
#include "proto/desktop.h"
#include "proto/desktop_extensions.h"
#include "proto/file_transfer.h"
#include "proto/system_info.h"
#include "proto/task_manager.h"
#include "proto/text_chat.h"

namespace proto {

void registerMetaTypes()
{
    // common.h
    qRegisterMetaType<proto::HostChannelId>("proto::HostChannelId");
    qRegisterMetaType<proto::SessionType>("proto::SessionType");
    qRegisterMetaType<proto::Version>("proto::Version");

    // desktop.h
    qRegisterMetaType<proto::KeyEvent>("proto::KeyEvent");
    qRegisterMetaType<proto::TextEvent>("proto::TextEvent");
    qRegisterMetaType<proto::MouseEvent>("proto::MouseEvent");
    qRegisterMetaType<proto::TouchEventPoint>("proto::TouchEventPoint");
    qRegisterMetaType<proto::TouchEvent>("proto::TouchEvent");
    qRegisterMetaType<proto::TouchEvent::TouchEventType>("proto::TouchEvent::TouchEventType");
    qRegisterMetaType<proto::ClipboardEvent>("proto::ClipboardEvent");
    qRegisterMetaType<proto::CursorShape>("proto::CursorShape");
    qRegisterMetaType<proto::CursorPosition>("proto::CursorPosition");
    qRegisterMetaType<proto::Size>("proto::Size");
    qRegisterMetaType<proto::Rect>("proto::Rect");
    qRegisterMetaType<proto::PixelFormat>("proto::PixelFormat");
    qRegisterMetaType<proto::VideoEncoding>("proto::VideoEncoding");
    qRegisterMetaType<proto::VideoPacketFormat>("proto::VideoPacketFormat");
    qRegisterMetaType<proto::VideoErrorCode>("proto::VideoErrorCode");
    qRegisterMetaType<proto::VideoPacket>("proto::VideoPacket");
    qRegisterMetaType<proto::AudioEncoding>("proto::AudioEncoding");
    qRegisterMetaType<proto::AudioPacket>("proto::AudioPacket");
    qRegisterMetaType<proto::AudioPacket::SamplingRate>("proto::AudioPacket::SamplingRate");
    qRegisterMetaType<proto::AudioPacket::BytesPerSample>("proto::AudioPacket::BytesPerSample");
    qRegisterMetaType<proto::AudioPacket::Channels>("proto::AudioPacket::Channels");
    qRegisterMetaType<proto::DesktopExtension>("proto::DesktopExtension");
    qRegisterMetaType<proto::DesktopCapabilities>("proto::DesktopCapabilities");
    qRegisterMetaType<proto::DesktopCapabilities::OsType>("proto::DesktopCapabilities::OsType");
    qRegisterMetaType<proto::DesktopCapabilities::Flag>("proto::DesktopCapabilities::Flag");
    qRegisterMetaType<proto::DesktopConfig>("proto::DesktopConfig");
    qRegisterMetaType<proto::HostToClient>("proto::HostToClient");
    qRegisterMetaType<proto::ClientToHost>("proto::ClientToHost");

    // desktop_extensions.h
    qRegisterMetaType<proto::Resolution>("proto::Resolution");
    qRegisterMetaType<proto::Point>("proto::Point");
    qRegisterMetaType<proto::Screen>("proto::Screen");
    qRegisterMetaType<proto::ScreenList>("proto::ScreenList");
    qRegisterMetaType<proto::PreferredSize>("proto::PreferredSize");
    qRegisterMetaType<proto::Pause>("proto::Pause");
    qRegisterMetaType<proto::PowerControl>("proto::PowerControl");
    qRegisterMetaType<proto::PowerControl::Action>("proto::PowerControl::Action");
    qRegisterMetaType<proto::VideoRecording>("proto::VideoRecording");
    qRegisterMetaType<proto::VideoRecording::Action>("proto::VideoRecording::Action");
    qRegisterMetaType<proto::ScreenType>("proto::ScreenType");
    qRegisterMetaType<proto::ScreenType::Type>("proto::ScreenType::Type");

    // file_transfer.h
    qRegisterMetaType<proto::DriveList>("proto::DriveList");
    qRegisterMetaType<proto::DriveList::Item>("proto::DriveList::Item");
    qRegisterMetaType<proto::DriveList::Item::Type>("proto::DriveList::Item::Type");
    qRegisterMetaType<proto::DriveListRequest>("proto::DriveListRequest");
    qRegisterMetaType<proto::FileList>("proto::FileList");
    qRegisterMetaType<proto::FileList::Item>("proto::FileList::Item");
    qRegisterMetaType<proto::FileListRequest>("proto::FileListRequest");
    qRegisterMetaType<proto::UploadRequest>("proto::UploadRequest");
    qRegisterMetaType<proto::DownloadRequest>("proto::DownloadRequest");
    qRegisterMetaType<proto::FilePacketRequest>("proto::FilePacketRequest");
    qRegisterMetaType<proto::FilePacket>("proto::FilePacket");
    qRegisterMetaType<proto::CreateDirectoryRequest>("proto::CreateDirectoryRequest");
    qRegisterMetaType<proto::RenameRequest>("proto::RenameRequest");
    qRegisterMetaType<proto::RemoveRequest>("proto::RemoveRequest");
    qRegisterMetaType<proto::FileError>("proto::FileError");
    qRegisterMetaType<proto::FileReply>("proto::FileReply");
    qRegisterMetaType<proto::FileRequest>("proto::FileRequest");

    // system_info.h
    qRegisterMetaType<proto::system_info::SystemInfoRequest>("proto::system_info::SystemInfoRequest");
    qRegisterMetaType<proto::system_info::SystemInfo>("proto::system_info::SystemInfo");

    // task_manager.h
    qRegisterMetaType<proto::task_manager::ClientToHost>("proto::task_manager::ClientToHost");
    qRegisterMetaType<proto::task_manager::HostToClient>("proto::task_manager::HostToClient");

    // text_chat.h
    qRegisterMetaType<proto::TextChat>("proto::TextChat");
}

} // namespace proto
