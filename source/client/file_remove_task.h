//
// PROJECT:         Aspia
// FILE:            client/file_remove_task.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVE_TASK_H
#define _ASPIA_CLIENT__FILE_REMOVE_TASK_H

#include <QString>

namespace aspia {

class FileRemoveTask
{
public:
    FileRemoveTask(const QString& path, bool is_directory);

    FileRemoveTask(const FileRemoveTask& other);
    FileRemoveTask& operator=(const FileRemoveTask& other);

    FileRemoveTask(FileRemoveTask&& other) noexcept;
    FileRemoveTask& operator=(FileRemoveTask&& other) noexcept;

    ~FileRemoveTask();

    const QString& path() const { return path_; }
    bool isDirectory() const { return is_directory_; }

private:
    QString path_;
    bool is_directory_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVE_TASK_H
