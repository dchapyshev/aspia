//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_code.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_CODE_H
#define _ASPIA_UI__STATUS_CODE_H

#include "proto/file_transfer_session.pb.h"
#include "proto/status.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

static INLINE CString StatusCodeToString(proto::Status status)
{
    UINT resource_id = status + IDS_STATUS_FIRST;

    if (!Status_IsValid(status))
        resource_id = IDS_STATUS_UNKNOWN;

    CString text;
    text.LoadStringW(resource_id);
    return text;
}

static INLINE CString RequestStatusCodeToString(proto::RequestStatus status)
{
    UINT resource_id = status + IDS_REQUEST_STATUS_FIRST;

    if (!RequestStatus_IsValid(status))
        resource_id = IDS_REQUEST_STATUS_UNKNOWN;

    CString text;
    text.LoadStringW(resource_id);
    return text;
}

} // namespace aspia

#endif // _ASPIA_UI__STATUS_CODE_H
