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

#ifndef BASE__FILES__FILE_PATH_WATCHER_H
#define BASE__FILES__FILE_PATH_WATCHER_H

#include "base/macros_magic.h"

#include <filesystem>
#include <functional>
#include <memory>

namespace base {

class TaskRunner;

// This class lets you register interest in changes on a std::filesystem::path. The callback will
// get called whenever the file or directory referenced by the std::filesystem::path is changed,
// including created or deleted. Due to limitations in the underlying OS APIs, FilePathWatcher has
// slightly different semantics on OS X than on Windows or Linux. FilePathWatcher on Linux and
// Windows will detect modifications to files in a watched directory. FilePathWatcher on Mac will
// detect the creation and deletion of files in a watched directory, but will not detect
// modifications to those files. See file_path_watcher_kqueue.cc for details.
//
// Must be destroyed on the sequence that invokes watch().
class FilePathWatcher
{
public:
    // Callback type for watch(). |path| points to the file that was updated, and |error| is true
    // if the platform specific code detected an error. In that case, the callback won't be invoked
    // again.
    using Callback = std::function<void(const std::filesystem::path& path, bool error)>;

    // Used internally to encapsulate different members on different platforms.
    class PlatformDelegate
    {
    public:
        explicit PlatformDelegate(std::shared_ptr<TaskRunner>& task_runner);
        virtual ~PlatformDelegate();

        // Start watching for the given |path| and notify |delegate| about changes.
        virtual bool watch(const std::filesystem::path& path,
                           bool recursive,
                           const Callback& callback) = 0;

        // Stop watching. This is called from FilePathWatcher's dtor in order to allow to shut down
        // properly while the object is still alive.
        virtual void cancel() = 0;

    protected:
        friend class FilePathWatcher;

        std::shared_ptr<TaskRunner> taskRunner() const
        {
            return task_runner_;
        }

        // Must be called before the PlatformDelegate is deleted.
        void setCancelled()
        {
            cancelled_ = true;
        }

        bool isCancelled() const
        {
            return cancelled_;
        }

    private:
        std::shared_ptr<TaskRunner> task_runner_;
        bool cancelled_;

        DISALLOW_COPY_AND_ASSIGN(PlatformDelegate);
    };

    explicit FilePathWatcher(std::shared_ptr<TaskRunner>& task_runner);
    ~FilePathWatcher();

    // Returns true if the platform and OS version support recursive watches.
    static bool recursiveWatchAvailable();

    // Invokes |callback| whenever updates to |path| are detected. This should be called at most
    // once. Set |recursive| to true to watch |path| and its children. The callback will be invoked
    // on the same sequence. Returns true on success.
    //
    // On POSIX, this must be called from a thread that supports FileDescriptorWatcher.
    //
    // Recursive watch is not supported on all platforms and file systems. Watch() will return
    // false in the case of failure.
    bool watch(const std::filesystem::path& path, bool recursive, const Callback& callback);

private:
    std::unique_ptr<PlatformDelegate> impl_;

    DISALLOW_COPY_AND_ASSIGN(FilePathWatcher);
};

} // namespace base

#endif // BASE__FILES__FILE_PATH_WATCHER_H
