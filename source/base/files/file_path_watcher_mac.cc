//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/files/file_path_watcher.h"

#include "base/logging.h"

namespace base {

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate
{
public:
    FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner);
    ~FilePathWatcherImpl() override;

    bool watch(const std::filesystem::path& path,
               bool recursive,
               const FilePathWatcher::Callback& callback) override;
    void cancel() override;

private:
    DISALLOW_COPY_AND_ASSIGN(FilePathWatcherImpl);
};

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner)
    : FilePathWatcher::PlatformDelegate(std::move(task_runner))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::~FilePathWatcherImpl() = default;

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::watch(const std::filesystem::path& /* path */,
                                bool /* recursive */,
                                const FilePathWatcher::Callback& /* callback */)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::cancel()
{
    NOTIMPLEMENTED();
}

} // namespace

//--------------------------------------------------------------------------------------------------
FilePathWatcher::FilePathWatcher(std::shared_ptr<TaskRunner> task_runner)
{
    impl_ = std::make_shared<FilePathWatcherImpl>(std::move(task_runner));
}

} // namespace base
