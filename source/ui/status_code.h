//
// PROJECT:         Aspia
// FILE:            ui/status_code.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_CODE_H
#define _ASPIA_UI__STATUS_CODE_H

#include "proto/file_transfer_session.pb.h"
#include "proto/status.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

namespace aspia {

CString StatusCodeToString(proto::Status status);
CString RequestStatusCodeToString(proto::RequestStatus status);

} // namespace aspia

#endif // _ASPIA_UI__STATUS_CODE_H
