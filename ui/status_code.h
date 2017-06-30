//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_code.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_CODE_H
#define _ASPIA_UI__STATUS_CODE_H

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
    UINT resource_id = status + IDS_STATUS_MIN;

    if (!Status_IsValid(status))
        resource_id = IDS_STATUS_UNKNOWN;

    CString text;
    text.LoadStringW(resource_id);
    return text;
}

} // namespace aspia

#endif // _ASPIA_UI__STATUS_CODE_H
