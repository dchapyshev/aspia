//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_code.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_CODE_H
#define _ASPIA_UI__STATUS_CODE_H

#include "proto/status.pb.h"
#include "ui/base/module.h"
#include "ui/resource.h"

namespace aspia {

static INLINE std::wstring StatusCodeToString(const UiModule& module, proto::Status status)
{
    UINT resource_id = status + IDS_STATUS_MIN;

    if (!Status_IsValid(status))
        resource_id = IDS_STATUS_UNKNOWN;

    return module.string(resource_id);
}

} // namespace aspia

#endif // _ASPIA_UI__STATUS_CODE_H
