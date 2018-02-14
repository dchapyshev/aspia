//
// PROJECT:         Aspia
// FILE:            base/files/file_associations.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__FILE_ASSOCIATIONS_H
#define _ASPIA_BASE__FILES__FILE_ASSOCIATIONS_H

#include "base/command_line.h"
#include "base/macros.h"

namespace aspia {

class FileAssociations
{
public:
    static bool Add(const CommandLine& command_line,
                    const WCHAR* extension,
                    const WCHAR* description,
                    int icon_index);

    static bool Remove(const WCHAR* extension);

private:
    DISALLOW_COPY_AND_ASSIGN(FileAssociations);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILES__FILE_ASSOCIATIONS_H
