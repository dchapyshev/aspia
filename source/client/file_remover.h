//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_remover.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVER_H
#define _ASPIA_CLIENT__FILE_REMOVER_H

#include "base/files/file_path.h"
#include "client/file_task_queue_builder.h"
#include "proto/file_transfer_session_message.pb.h"

namespace aspia {

class FileRemover
{
public:
    virtual ~FileRemover() = default;

    virtual void Start(const FilePath& source_path,
                       const FilePath& target_path,
                       const FileTaskQueueBuilder::FileList& file_list) = 0;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVER_H
