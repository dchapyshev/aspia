//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_FILES_FILE_DESCRIPTOR_WATCHER_POSIX_H
#define BASE_FILES_FILE_DESCRIPTOR_WATCHER_POSIX_H

#include <QtGlobal>

#include <functional>
#include <memory>

namespace base {

class FileDescriptorWatcher
{
public:
    FileDescriptorWatcher();
    ~FileDescriptorWatcher();

    enum class Mode
    {
        WATCH_READ,
        WATCH_WRITE
    };

    using Callback = std::function<void()>;

    void startWatching(int fd, Mode mode, const Callback& callback);

private:
    class Watcher;
    std::unique_ptr<Watcher> impl_;

    Q_DISABLE_COPY(FileDescriptorWatcher)
};

} // namespace base

#endif // BASE_FILES_FILE_DESCRIPTOR_WATCHER_POSIX_H
