//
// PROJECT:         Aspia
// FILE:            client/file_remove_task.h
// LICENSE:         GNU Lesser General Public License 2.1
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
    FileRemoveTask(QString&& path, bool is_directory) noexcept;

    FileRemoveTask(const FileRemoveTask& other);
    FileRemoveTask& operator=(const FileRemoveTask& other);

    FileRemoveTask(FileRemoveTask&& other) noexcept;
    FileRemoveTask& operator=(FileRemoveTask&& other) noexcept;

    ~FileRemoveTask();

    const QString& path() const;
    bool isDirectory() const;

private:
    QString path_;
    bool is_directory_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVE_TASK_H
