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
    FileTransferTask(const QString& source_path,
                     const QString& target_path,
                     bool is_directory,
                     qint64 size);

    FileTransferTask(const FileTransferTask& other) = default;
    FileTransferTask& operator=(const FileTransferTask& other) = default;

    FileTransferTask(FileTransferTask&& other) noexcept;

    FileTransferTask& operator=(FileTransferTask&& other) noexcept;

    ~FileTransferTask() = default;

    const QString& sourcePath() const { return source_path_; }
    const QString& targetPath() const { return target_path_; }
    bool isDirectory() const { return is_directory_; }
    qint64 size() const { return size_; }

    bool overwrite() const { return overwrite_; }
    void setOverwrite(bool value) { overwrite_ = value; }

private:
    QString source_path_;
    QString target_path_;
    bool is_directory_;
    qint64 size_;
    bool overwrite_ = false;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_TASK_H
