//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_status_code.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_status_code.h"
#include "ui/resource.h"

namespace aspia {

CString RequestStatusCodeToString(proto::RequestStatus status)
{
    UINT resource_id;

    switch (status)
    {
        case proto::REQUEST_STATUS_SUCCESS:
            resource_id = IDS_REQUEST_STATUS_SUCCESS;
            break;

        case proto::REQUEST_STATUS_INVALID_PATH_NAME:
            resource_id = IDS_REQUEST_STATUS_INVALID_PATH_NAME;
            break;

        case proto::REQUEST_STATUS_PATH_NOT_FOUND:
            resource_id = IDS_REQUEST_STATUS_PATH_NOT_FOUND;
            break;

        case proto::REQUEST_STATUS_PATH_ALREADY_EXISTS:
            resource_id = IDS_REQUEST_STATUS_PATH_ALREADY_EXISTS;
            break;

        case proto::REQUEST_STATUS_NO_DRIVES_FOUND:
            resource_id = IDS_REQUEST_STATUS_NO_DRIVES_FOUND;
            break;

        case proto::REQUEST_STATUS_DISK_FULL:
            resource_id = IDS_REQUEST_STATUS_DISK_FULL;
            break;

        case proto::REQUEST_STATUS_ACCESS_DENIED:
            resource_id = IDS_REQUEST_STATUS_ACCESS_DENIED;
            break;

        case proto::REQUEST_STATUS_FILE_OPEN_ERROR:
            resource_id = IDS_REQUEST_STATUS_FILE_OPEN_ERROR;
            break;

        case proto::REQUEST_STATUS_FILE_CREATE_ERROR:
            resource_id = IDS_REQUEST_STATUS_FILE_CREATE_ERROR;
            break;

        case proto::REQUEST_STATUS_FILE_WRITE_ERROR:
            resource_id = IDS_REQUEST_STATUS_FILE_WRITE_ERROR;
            break;

        case proto::REQUEST_STATUS_FILE_READ_ERROR:
            resource_id = IDS_REQUEST_STATUS_FILE_READ_ERROR;
            break;

        default:
            resource_id = IDS_REQUEST_STATUS_UNKNOWN;
            break;
    }

    CString text;
    text.LoadStringW(resource_id);

    return text;
}

} // namespace aspia
