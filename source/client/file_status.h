//
// PROJECT:         Aspia
// FILE:            client/file_status.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_STATUS_H
#define _ASPIA_CLIENT__FILE_STATUS_H

#include <QString>

#include "protocol/file_transfer_session.pb.h"

namespace aspia {

QString fileStatusToString(proto::file_transfer::Status status);

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_STATUS_H
