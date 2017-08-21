//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_constants.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_CONSTANTS_H
#define _ASPIA_CLIENT__FILE_CONSTANTS_H

namespace aspia {

enum class FileAction
{
    ASK         = 0,
    ABORT       = 1,
    SKIP        = 2,
    SKIP_ALL    = 3,
    REPLACE     = 4,
    REPLACE_ALL = 5
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_CONSTANTS_H
