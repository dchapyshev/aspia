//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/file_request_factory.h"

#include "base/logging.h"
#include "proto/file_transfer.pb.h"

namespace client {

FileRequestFactory::FileRequestFactory(
    std::shared_ptr<common::FileRequestProducerProxy>& producer_proxy,
    common::FileTaskTarget target)
    : producer_proxy_(producer_proxy),
      target_(target)
{
    DCHECK(producer_proxy_);
    DCHECK(target_ == common::FileTaskTarget::LOCAL || target_ == common::FileTaskTarget::REMOTE);
}

FileRequestFactory::~FileRequestFactory() = default;

std::shared_ptr<common::FileRequest> FileRequestFactory::driveListRequest()
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_drive_list_request()->set_dummy(1);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::fileListRequest(const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_file_list_request()->set_path(path);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::createDirectoryRequest(
    const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_create_directory_request()->set_path(path);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::renameRequest(
    const std::string& old_name, const std::string& new_name)
{
    auto request = std::make_unique<proto::FileRequest>();

    proto::RenameRequest* rename_request = request->mutable_rename_request();
    rename_request->set_old_name(old_name);
    rename_request->set_new_name(new_name);

    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::removeRequest(const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_remove_request()->set_path(path);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::downloadRequest(
    const std::string& file_path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_download_request()->set_path(file_path);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::uploadRequest(
    const std::string& file_path, bool overwrite)
{
    auto request = std::make_unique<proto::FileRequest>();

    proto::UploadRequest* upload_request = request->mutable_upload_request();
    upload_request->set_path(file_path);
    upload_request->set_overwrite(overwrite);

    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::packetRequest(uint32_t flags)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_packet_request()->set_flags(flags);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::packet(const proto::FilePacket& packet)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_packet()->CopyFrom(packet);
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::packet(
    std::unique_ptr<proto::FilePacket> packet)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->set_allocated_packet(packet.release());
    return makeRequest(std::move(request));
}

std::shared_ptr<common::FileRequest> FileRequestFactory::makeRequest(
    std::unique_ptr<proto::FileRequest> request)
{
    return std::make_shared<common::FileRequest>(producer_proxy_, std::move(request), target_);
}

} // namespace client
