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

#ifndef COMMON__FILE_REQUEST_H
#define COMMON__FILE_REQUEST_H

#include "base/macros_magic.h"
#include "common/file_task_target.h"

#include <memory>

namespace proto {
class FileRequest;
class FileReply;
} // namespace proto

namespace common {

class FileRequestFactory;
class FileRequestProducerProxy;

class FileRequest : public std::enable_shared_from_this<FileRequest>
{
public:
    FileRequest(std::shared_ptr<FileRequestProducerProxy> producer_proxy,
                std::unique_ptr<proto::FileRequest> request,
                FileTaskTarget target);
    virtual ~FileRequest();

    // Returns the target of the current request. It can be a local computer or a remote computer.
    FileTaskTarget target() const { return target_; }

    // Returns the data of the current request.
    const proto::FileRequest& request() const;

    // Returns reply data for the current request.
    // If method setReply has not been called and data has not been set, an empty reply will be
    // returned.
    const proto::FileReply& reply() const;

    // Sets the reply to the current request. The sender will be notified of this reply.
    void setReply(std::unique_ptr<proto::FileReply> reply);

private:
    std::shared_ptr<FileRequestProducerProxy> producer_proxy_;
    const FileTaskTarget target_;

    std::unique_ptr<proto::FileRequest> request_;
    std::unique_ptr<proto::FileReply> reply_;

    DISALLOW_COPY_AND_ASSIGN(FileRequest);
};

} // namespace common

#endif // COMMON__FILE_REQUEST_H
