//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_remover_remote.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVER_REMOTE_H
#define _ASPIA_CLIENT__FILE_REMOVER_REMOTE_H

#include "base/macros.h"
#include "client/file_remover.h"

namespace aspia {

class FileRemoverRemote : public FileRemover
{
public:
    FileRemoverRemote() = default;
    ~FileRemoverRemote() = default;

    void Start(const FilePath& source_path,
               const FilePath& target_path,
               const FileTaskQueueBuilder::FileList& file_list) override;

private:
    DISALLOW_COPY_AND_ASSIGN(FileRemoverRemote);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVER_REMOTE_H
