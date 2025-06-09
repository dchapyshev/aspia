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

#include "proto/desktop.h"
#include "proto/file_transfer.h"
#include "proto/host_internal.h"
#include "proto/peer_common.h"
#include "proto/router_admin.h"
#include "proto/system_info.h"
#include "proto/task_manager.h"
#include "proto/text_chat.h"

namespace proto {

void registerMetaTypes()
{
    // common.h
    qRegisterMetaType<proto::peer::HostChannelId>("proto::HostChannelId");
    qRegisterMetaType<proto::peer::SessionType>("proto::SessionType");
    qRegisterMetaType<proto::peer::Version>("proto::Version");

    // desktop.h
    qRegisterMetaType<proto::desktop::KeyEvent>("proto::KeyEvent");
    qRegisterMetaType<proto::desktop::TextEvent>("proto::TextEvent");
    qRegisterMetaType<proto::desktop::MouseEvent>("proto::MouseEvent");
    qRegisterMetaType<proto::desktop::TouchEventPoint>("proto::TouchEventPoint");
    qRegisterMetaType<proto::desktop::TouchEvent>("proto::TouchEvent");
    qRegisterMetaType<proto::desktop::TouchEvent::TouchEventType>("proto::TouchEvent::TouchEventType");
    qRegisterMetaType<proto::desktop::ClipboardEvent>("proto::ClipboardEvent");
    qRegisterMetaType<proto::desktop::CursorShape>("proto::CursorShape");
    qRegisterMetaType<proto::desktop::CursorPosition>("proto::CursorPosition");
    qRegisterMetaType<proto::desktop::Size>("proto::Size");
    qRegisterMetaType<proto::desktop::Rect>("proto::Rect");
    qRegisterMetaType<proto::desktop::PixelFormat>("proto::PixelFormat");
    qRegisterMetaType<proto::desktop::VideoEncoding>("proto::VideoEncoding");
    qRegisterMetaType<proto::desktop::VideoPacketFormat>("proto::VideoPacketFormat");
    qRegisterMetaType<proto::desktop::VideoErrorCode>("proto::VideoErrorCode");
    qRegisterMetaType<proto::desktop::VideoPacket>("proto::VideoPacket");
    qRegisterMetaType<proto::desktop::AudioEncoding>("proto::AudioEncoding");
    qRegisterMetaType<proto::desktop::AudioPacket>("proto::AudioPacket");
    qRegisterMetaType<proto::desktop::AudioPacket::SamplingRate>("proto::AudioPacket::SamplingRate");
    qRegisterMetaType<proto::desktop::AudioPacket::BytesPerSample>("proto::AudioPacket::BytesPerSample");
    qRegisterMetaType<proto::desktop::AudioPacket::Channels>("proto::AudioPacket::Channels");
    qRegisterMetaType<proto::desktop::Extension>("proto::DesktopExtension");
    qRegisterMetaType<proto::desktop::Capabilities>("proto::DesktopCapabilities");
    qRegisterMetaType<proto::desktop::Capabilities::OsType>("proto::DesktopCapabilities::OsType");
    qRegisterMetaType<proto::desktop::Capabilities::Flag>("proto::DesktopCapabilities::Flag");
    qRegisterMetaType<proto::desktop::Config>("proto::DesktopConfig");
    qRegisterMetaType<proto::desktop::HostToClient>("proto::HostToClient");
    qRegisterMetaType<proto::desktop::ClientToHost>("proto::ClientToHost");
    qRegisterMetaType<proto::desktop::Point>("proto::Point");
    qRegisterMetaType<proto::desktop::Screen>("proto::Screen");
    qRegisterMetaType<proto::desktop::ScreenList>("proto::ScreenList");
    qRegisterMetaType<proto::desktop::PreferredSize>("proto::PreferredSize");
    qRegisterMetaType<proto::desktop::Pause>("proto::Pause");
    qRegisterMetaType<proto::desktop::PowerControl>("proto::PowerControl");
    qRegisterMetaType<proto::desktop::PowerControl::Action>("proto::PowerControl::Action");
    qRegisterMetaType<proto::desktop::VideoRecording>("proto::VideoRecording");
    qRegisterMetaType<proto::desktop::VideoRecording::Action>("proto::VideoRecording::Action");
    qRegisterMetaType<proto::desktop::ScreenType>("proto::ScreenType");
    qRegisterMetaType<proto::desktop::ScreenType::Type>("proto::ScreenType::Type");

    // file_transfer.h
    qRegisterMetaType<proto::file_transfer::DriveList>("proto::DriveList");
    qRegisterMetaType<proto::file_transfer::DriveList::Item>("proto::DriveList::Item");
    qRegisterMetaType<proto::file_transfer::DriveList::Item::Type>("proto::DriveList::Item::Type");
    qRegisterMetaType<proto::file_transfer::DriveListRequest>("proto::DriveListRequest");
    qRegisterMetaType<proto::file_transfer::List>("proto::FileList");
    qRegisterMetaType<proto::file_transfer::List::Item>("proto::FileList::Item");
    qRegisterMetaType<proto::file_transfer::ListRequest>("proto::FileListRequest");
    qRegisterMetaType<proto::file_transfer::UploadRequest>("proto::UploadRequest");
    qRegisterMetaType<proto::file_transfer::DownloadRequest>("proto::DownloadRequest");
    qRegisterMetaType<proto::file_transfer::PacketRequest>("proto::FilePacketRequest");
    qRegisterMetaType<proto::file_transfer::Packet>("proto::FilePacket");
    qRegisterMetaType<proto::file_transfer::CreateDirectoryRequest>("proto::CreateDirectoryRequest");
    qRegisterMetaType<proto::file_transfer::RenameRequest>("proto::RenameRequest");
    qRegisterMetaType<proto::file_transfer::RemoveRequest>("proto::RemoveRequest");
    qRegisterMetaType<proto::file_transfer::ErrorCode>("proto::FileError");
    qRegisterMetaType<proto::file_transfer::Reply>("proto::FileReply");
    qRegisterMetaType<proto::file_transfer::Request>("proto::FileRequest");

    // host_internal.h
    qRegisterMetaType<proto::internal::Credentials>("proto::internal::Credentials");
    qRegisterMetaType<proto::internal::CredentialsRequest::Type>("proto::internal::CredentialsRequest::Type");
    qRegisterMetaType<proto::internal::RouterState>("proto::internal::RouterState");
    qRegisterMetaType<proto::internal::ConnectConfirmationRequest>("proto::internal::ConnectConfirmationRequest");

    // router_admin.h
    qRegisterMetaType<proto::router::User>("proto::User");

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
