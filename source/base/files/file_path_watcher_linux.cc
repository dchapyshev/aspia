//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
    FilePathWatcherImpl();
    ~FilePathWatcherImpl() override;

    bool watch(const std::filesystem::path& path,
               bool recursive,
               const Callback& callback) override;
    void cancel() override;
};

bool FilePathWatcherImpl::watch(const std::filesystem::path& /* path */,
                                bool /* recursive */,
                                const Callback& /* callback */)
{
    NOTIMPLEMENTED();
    return false;
}

void FilePathWatcherImpl::cancel()
{
    NOTIMPLEMENTED();
}

} // namespace

FilePathWatcher::FilePathWatcher(std::shared_ptr<TaskRunner> task_runner)
{
    impl_ = std::make_unique<FilePathWatcherImpl>(std::move(task_runner));
}

} // namespace base
