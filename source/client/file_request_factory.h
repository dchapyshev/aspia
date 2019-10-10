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

#ifndef CLIENT__FILE_REQUEST_FACTORY_H
#define CLIENT__FILE_REQUEST_FACTORY_H

#include "common/file_request.h"

#include <string>

namespace proto {
class FilePacket;
} // namespace proto

namespace client {

class FileRequestFactory
{
public:
    FileRequestFactory(std::shared_ptr<common::FileRequestProducerProxy> producer_proxy,
                       common::FileTaskTarget target);
    ~FileRequestFactory();

    common::FileTaskTarget target() const { return target_; }

    std::shared_ptr<common::FileRequest> driveListRequest();
    std::shared_ptr<common::FileRequest> fileListRequest(const std::string& path);
    std::shared_ptr<common::FileRequest> createDirectoryRequest(const std::string& path);
    std::shared_ptr<common::FileRequest> renameRequest(
        const std::string& old_name, const std::string& new_name);
    std::shared_ptr<common::FileRequest> removeRequest(const std::string& path);
    std::shared_ptr<common::FileRequest> downloadRequest(const std::string& file_path);
    std::shared_ptr<common::FileRequest> uploadRequest(const std::string& file_path, bool overwrite);
    std::shared_ptr<common::FileRequest> packetRequest(uint32_t flags);
    std::shared_ptr<common::FileRequest> packet(const proto::FilePacket& packet);
    std::shared_ptr<common::FileRequest> packet(std::unique_ptr<proto::FilePacket> packet);

private:
    std::shared_ptr<common::FileRequest> makeRequest(std::unique_ptr<proto::FileRequest> request);

    std::shared_ptr<common::FileRequestProducerProxy> producer_proxy_;
    const common::FileTaskTarget target_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestFactory);
};

} // namespace client

#endif // CLIENT__FILE_REQUEST_FACTORY_H
