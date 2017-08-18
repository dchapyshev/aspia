//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_remover_local.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVER_LOCAL_H
#define _ASPIA_CLIENT__FILE_REMOVER_LOCAL_H

#include "base/macros.h"
#include "client/file_remover.h"

namespace aspia {

class FileRemoverLocal : public FileRemover
{
public:
    FileRemoverLocal() = default;
    ~FileRemoverLocal() = default;

    void Start(const FilePath& source_path,
               const FilePath& target_path,
               const FileList& file_list) override;

private:
    DISALLOW_COPY_AND_ASSIGN(FileRemoverLocal);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVER_LOCAL_H
