//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/system_info_request.h"

#include "base/guid.h"

namespace aspia {

SystemInfoRequest::SystemInfoRequest(proto::system_info::Request&& request)
    : request_(std::move(request))
{
    request_.set_request_uuid(Guid::create());
}

// static
SystemInfoRequest* SystemInfoRequest::categoryList()
{
    proto::system_info::Request request;
    request.mutable_category_list_request()->set_dummy(1);
    return new SystemInfoRequest(std::move(request));
}

// static
SystemInfoRequest* SystemInfoRequest::category(const QString& uuid, const QByteArray& params)
{
    proto::system_info::Request request;
    request.mutable_category_request()->set_uuid(uuid.toStdString());
    request.mutable_category_request()->set_params(params.toStdString());
    return new SystemInfoRequest(std::move(request));
}

} // namespace aspia
