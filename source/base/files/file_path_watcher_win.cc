//
// Aspia Project
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

#include "base/files/file_path_watcher.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/strings/unicode.h"
#include "base/win/object_watcher.h"

namespace base {

namespace {

class FilePathWatcherImpl final
    : public FilePathWatcher::PlatformDelegate,
      public win::ObjectWatcher::Delegate
{
public:
    FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner);
    ~FilePathWatcherImpl() final;

    // FilePathWatcher::PlatformDelegate implementation.
    bool watch(const std::filesystem::path& path,
               bool recursive,
               const FilePathWatcher::Callback& callback) final;
    void cancel() final;

    // win::ObjectWatcher::Delegate implementation.
    void onObjectSignaled(HANDLE object) final;

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using FileTime = std::filesystem::file_time_type;

    // Setup a watch handle for directory |dir|. Set |recursive| to true to watch the directory sub
    // trees. Returns true if no fatal error occurs. |handle| will receive the handle value if
    // |dir| is watchable, otherwise INVALID_HANDLE_VALUE.
    static bool setupWatchHandle(const std::filesystem::path& dir,
                                 bool recursive,
                                 HANDLE* handle);

    // (Re-)Initialize the watch handle.
    bool updateWatch();

    // Destroy the watch handle.
    void destroyWatch();

    // Callback to notify upon changes.
    FilePathWatcher::Callback callback_;

    // Path we're supposed to watch (passed to callback).
    std::filesystem::path target_;

    // Set to true in the destructor.
    bool* was_deleted_ptr_ = nullptr;

    // Handle for FindFirstChangeNotification.
    HANDLE handle_ = INVALID_HANDLE_VALUE;

    // ObjectWatcher to watch handle_ for events.
    win::ObjectWatcher watcher_;

    // Set to true to watch the sub trees of the specified directory file path.
    bool recursive_watch_ = false;

    // Keep track of the last modified time of the file.  We use nulltime to represent the file not
    // existing.
    FileTime last_modified_;

    // The time at which we processed the first notification with the |last_modified_| time stamp.
    TimePoint first_notification_;

    DISALLOW_COPY_AND_ASSIGN(FilePathWatcherImpl);
};

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner)
    : FilePathWatcher::PlatformDelegate(task_runner),
      watcher_(task_runner)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::~FilePathWatcherImpl()
{
    DCHECK(!taskRunner() || taskRunner()->belongsToCurrentThread());

    if (was_deleted_ptr_)
        *was_deleted_ptr_ = true;
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::watch(const std::filesystem::path& path,
                                bool recursive,
                                const FilePathWatcher::Callback& callback)
{
    DCHECK(target_.empty()); // Can only watch one path.

    callback_ = callback;
    target_ = path;
    recursive_watch_ = recursive;

    std::error_code error_code;

    FileTime last_modified = std::filesystem::last_write_time(target_, error_code);
    if (!error_code)
    {
        last_modified_ = last_modified;
        first_notification_ = Clock::now();
    }
    else
    {
        LOG(LS_ERROR) << "std::filesystem::last_write_time failed: "
                      << base::utf16FromLocal8Bit(error_code.message());
    }

    if (!updateWatch())
        return false;

    watcher_.startWatchingOnce(handle_, this);
    return true;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::cancel()
{
    if (!callback_)
    {
        // Watch was never called, or the |task_runner_| has already quit.
        setCancelled();
        return;
    }

    DCHECK(taskRunner()->belongsToCurrentThread());
    setCancelled();

    if (handle_ != INVALID_HANDLE_VALUE)
        destroyWatch();

    callback_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::onObjectSignaled(HANDLE object)
{
    DCHECK(taskRunner()->belongsToCurrentThread());
    DCHECK_EQ(object, handle_);
    DCHECK(!was_deleted_ptr_);

    bool was_deleted = false;
    was_deleted_ptr_ = &was_deleted;

    if (!updateWatch())
    {
        callback_(target_, true /* error */);
        return;
    }

    // Check whether the event applies to |target_| and notify the callback.
    bool file_exists = false;

    std::error_code error_code;

    FileTime last_modified = std::filesystem::last_write_time(target_, error_code);
    if (!error_code)
        file_exists = true;

    if (recursive_watch_)
    {
        // Only the mtime of |target_| is tracked but in a recursive watch, some other file or
        // directory may have changed so all notifications are passed through. It is possible to
        // figure out which file changed using ReadDirectoryChangesW() instead of
        // FindFirstChangeNotification(), but that function is quite complicated:
        // http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html
        callback_(target_, false);
    }
    else if (file_exists && (last_modified_ == FileTime() || last_modified_ != last_modified))
    {
        last_modified_ = last_modified;
        first_notification_ = Clock::now();
        callback_(target_, false);
    }
    else if (file_exists && last_modified_ == last_modified && first_notification_ != TimePoint())
    {
        // The target's last modification time is equal to what's on record. This means that either
        // an unrelated event occurred, or the target changed again (file modification times only
        // have a resolution of 1s). Comparing file modification times against the wall clock is
        // not reliable to find out whether the change is recent, since this code might just run too
        // late. Moreover, there's no guarantee that file modification time and wall clock times
        // come from the same source.
        //
        // Instead, the time at which the first notification carrying the current |last_notified_|
        // time stamp is recorded. Later notifications that find the same file modification time
        // only need to be forwarded until wall clock has advanced one second from the initial
        // notification. After that interval, client code is guaranteed to having seen the current
        // revision of the file.
        if (Clock::now() - first_notification_ > std::chrono::seconds(1))
        {
            // Stop further notifications for this |last_modification_| time stamp.
            first_notification_ = Clock::now();
        }
        callback_(target_, false);
    }
    else if (!file_exists && last_modified_ != FileTime())
    {
        last_modified_ = FileTime();
        callback_(target_, false);
    }

    // The watch may have been cancelled by the callback.
    if (!was_deleted)
    {
        watcher_.startWatchingOnce(handle_, this);
        was_deleted_ptr_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
// static
bool FilePathWatcherImpl::setupWatchHandle(const std::filesystem::path& dir,
                                           bool recursive,
                                           HANDLE* handle)
{
    static const DWORD flags = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY;

    *handle = FindFirstChangeNotificationW(dir.c_str(), recursive, flags);
    if (*handle != INVALID_HANDLE_VALUE)
    {
        // Make sure the handle we got points to an existing directory. It seems that windows
        // sometimes hands out watches to directories that are about to go away, but doesn't sent
        // notifications if that happens.
        DWORD attr = GetFileAttributesW(dir.c_str());

        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            FindCloseChangeNotification(*handle);
            *handle = INVALID_HANDLE_VALUE;
        }

        return true;
    }

    // If FindFirstChangeNotification failed because the target directory doesn't exist, access is
    // denied (happens if the file is already gone but there are still handles open), or the target
    // is not a directory, try the immediate parent directory instead.
    DWORD error_code = GetLastError();
    if (error_code != ERROR_FILE_NOT_FOUND &&
        error_code != ERROR_PATH_NOT_FOUND &&
        error_code != ERROR_ACCESS_DENIED &&
        error_code != ERROR_SHARING_VIOLATION &&
        error_code != ERROR_DIRECTORY)
    {
        DPLOG(LS_ERROR) << "FindFirstChangeNotification failed for " << dir;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::updateWatch()
{
    if (handle_ != INVALID_HANDLE_VALUE)
        destroyWatch();

    // Start at the target and walk up the directory chain until we succesfully create a watch
    // handle in |handle_|. |child_dirs| keeps a stack of child directories stripped from target,
    // in reverse order.
    std::vector<std::filesystem::path> child_dirs;
    std::filesystem::path watched_path(target_);

    while (true)
    {
        if (!setupWatchHandle(watched_path, recursive_watch_, &handle_))
            return false;

        // Break if a valid handle is returned. Try the parent directory otherwise.
        if (handle_ != INVALID_HANDLE_VALUE)
            break;

        // Abort if we hit the root directory.
        child_dirs.emplace_back(watched_path.parent_path());

        std::filesystem::path parent(watched_path);
        parent.remove_filename();

        if (parent == watched_path)
        {
            LOG(LS_ERROR) << "Reached the root directory";
            return false;
        }

        watched_path = parent;
    }

    // At this point, handle_ is valid. However, the bottom-up search that the above code performs
    // races against directory creation. So try to walk back down and see whether any children
    // appeared in the mean time.
    while (!child_dirs.empty())
    {
        watched_path = watched_path.append(child_dirs.back().c_str());
        child_dirs.pop_back();

        HANDLE temp_handle = INVALID_HANDLE_VALUE;

        if (!setupWatchHandle(watched_path, recursive_watch_, &temp_handle))
            return false;

        if (temp_handle == INVALID_HANDLE_VALUE)
            break;

        if (!FindCloseChangeNotification(handle_))
        {
            PLOG(LS_ERROR) << "FindCloseChangeNotification failed";
        }
        handle_ = temp_handle;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::destroyWatch()
{
    watcher_.stopWatching();

    if (!FindCloseChangeNotification(handle_))
    {
        PLOG(LS_ERROR) << "FindCloseChangeNotification failed";
    }
    handle_ = INVALID_HANDLE_VALUE;
}

}  // namespace

//--------------------------------------------------------------------------------------------------
FilePathWatcher::FilePathWatcher(std::shared_ptr<TaskRunner> task_runner)
{
    impl_ = std::make_shared<FilePathWatcherImpl>(std::move(task_runner));
}

} // namespace base
