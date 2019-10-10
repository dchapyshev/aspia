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

#include "common/file_request_producer_proxy.h"

#include "base/logging.h"

namespace common {

FileRequestProducerProxy::FileRequestProducerProxy(FileRequestProducer* request_producer)
    : request_producer_(request_producer)
{
    DCHECK(request_producer_);
}

FileRequestProducerProxy::~FileRequestProducerProxy()
{
    DCHECK(!request_producer_);
}

void FileRequestProducerProxy::dettach()
{
    request_producer_ = nullptr;
}

void FileRequestProducerProxy::onReply(std::shared_ptr<FileRequest> request)
{
    if (request_producer_)
        request_producer_->onReply(request);
}

} // namespace common
