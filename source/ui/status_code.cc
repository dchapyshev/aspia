//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_code.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/status_code.h"
#include "ui/resource.h"

namespace aspia {

CString StatusCodeToString(proto::Status status)
{
    UINT resource_id = status + IDS_STATUS_FIRST;

    if (!Status_IsValid(status))
        resource_id = IDS_STATUS_UNKNOWN;

    CString text;
    text.LoadStringW(resource_id);
    return text;
}

CString RequestStatusCodeToString(proto::RequestStatus status)
{
    UINT resource_id = status + IDS_REQUEST_STATUS_FIRST;

    if (!RequestStatus_IsValid(status))
        resource_id = IDS_REQUEST_STATUS_UNKNOWN;

    CString text;
    text.LoadStringW(resource_id);
    return text;
}

} // namespace aspia
