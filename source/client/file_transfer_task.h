//
// PROJECT:         Aspia
// FILE:            client/file_transfer_task.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_TASK_H
#define _ASPIA_CLIENT__FILE_TRANSFER_TASK_H

#include <QString>

namespace aspia {

class FileTransferTask
{
public:
    FileTransferTask(const QString& source_path, const QString& target_path, bool is_directory);

    FileTransferTask(const FileTransferTask& other);
    FileTransferTask& operator=(const FileTransferTask& other);

    FileTransferTask(FileTransferTask&& other) noexcept;
    FileTransferTask& operator=(FileTransferTask&& other) noexcept;

    ~FileTransferTask();

    const QString& sourcePath() const;
    const QString& targetPath() const;
    bool isDirectory() const;

private:
    QString source_path_;
    QString target_path_;
    bool is_directory_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_TASK_H
