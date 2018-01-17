//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_status_code.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_CODE_H
#define _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_CODE_H

#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

namespace aspia {

CString RequestStatusCodeToString(proto::file_transfer::Status status);

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_CODE_H
