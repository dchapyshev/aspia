//
// PROJECT:         Aspia
// FILE:            host/system_info_request.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/system_info_request.h"

#include <QUuid>

namespace aspia {

SystemInfoRequest::SystemInfoRequest(proto::system_info::Request&& request)
    : request_(std::move(request))
{
    QString request_uuid = QUuid::createUuid().toString();
    request_.set_request_uuid(request_uuid.toStdString());
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
