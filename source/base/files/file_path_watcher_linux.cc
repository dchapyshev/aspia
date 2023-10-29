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
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/threading/simple_thread.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace base {

namespace {

class FilePathWatcherImpl;
class InotifyReader;

// The /proc path to max_user_watches.
constexpr char kInotifyMaxUserWatchesPath[] = "/proc/sys/fs/inotify/max_user_watches";

// This is a soft limit. If there are more than |kExpectedFilePathWatches| FilePathWatchers for a
// user, than they might affect each other's inotify watchers limit.
constexpr size_t kExpectedFilePathWatchers = 16u;

// The default max inotify watchers limit per user, if reading
// /proc/sys/fs/inotify/max_user_watches fails.
constexpr size_t kDefaultInotifyMaxUserWatches = 8192u;

// Used by test to override inotify watcher limit.
size_t g_override_max_inotify_watches = 0u;

bool isLink(const std::filesystem::path& file_path)
{
    struct stat st;
    // If we can't lstat the file, it's safe to assume that the file won't at
    // least be a 'followable' link.
    if (lstat(file_path.c_str(), &st) != 0)
        return false;
    return S_ISLNK(st.st_mode);
}

bool readSymbolicLink(const std::filesystem::path& symlink_path, std::filesystem::path* target_path)
{
    char buf[256];
    ssize_t count = ::readlink(symlink_path.c_str(), buf, std::size(buf));
    if (count <= 0)
    {
        target_path->clear();
        return false;
    }

    *target_path = std::filesystem::path(std::filesystem::path::string_type(buf, count));
    return true;
}

bool isPathExists(const std::filesystem::path& path)
{
    std::error_code ignored_error;
    return std::filesystem::exists(path, ignored_error);
}

// Get the maximum number of inotify watches can be used by a FilePathWatcher instance. This is
// based on /proc/sys/fs/inotify/max_user_watches entry.
size_t maxNumberOfInotifyWatches()
{
    static const size_t max = []()
    {
        size_t max_number_of_inotify_watches = 0u;

        std::ifstream in(kInotifyMaxUserWatchesPath);
        if (!in.is_open() || !(in >> max_number_of_inotify_watches))
        {
            LOG(LS_ERROR) << "Failed to read " << kInotifyMaxUserWatchesPath;
            return kDefaultInotifyMaxUserWatches / kExpectedFilePathWatchers;
        }

        return max_number_of_inotify_watches / kExpectedFilePathWatchers;
    }();

    return g_override_max_inotify_watches ? g_override_max_inotify_watches : max;
}

class InotifyReaderThreadDelegate final : public SimpleThread
{
public:
    explicit InotifyReaderThreadDelegate(int inotify_fd)
        : inotify_fd_(inotify_fd)
    {
        // Nothing
    }
    ~InotifyReaderThreadDelegate() override
    {
        stop();
    }

    void startReader();

private:
    void run();

    const int inotify_fd_;

    DISALLOW_COPY_AND_ASSIGN(InotifyReaderThreadDelegate);
};

// Singleton to manage all inotify watches.
class InotifyReader
{
public:
    InotifyReader();

    using Watch = int;  // Watch descriptor used by addWatch() and removeWatch().
    static constexpr Watch kInvalidWatch = -1;
    static constexpr Watch kWatchLimitExceeded = -2;

    static InotifyReader& instance();

    // Watch directory |path| for changes. |watcher| will be notified on each change. Returns
    // |kInvalidWatch| on failure.
    Watch addWatch(const std::filesystem::path& path, FilePathWatcherImpl* watcher);

    // Remove |watch| if it's valid.
    void removeWatch(Watch watch, FilePathWatcherImpl* watcher);

    // Invoked on "inotify_reader" thread to notify relevant watchers.
    void onInotifyEvent(const inotify_event* event);

    // Returns true if any paths are actively being watched.
    bool hasWatches();

private:
    // Record of watchers tracked for watch descriptors.
    struct WatcherEntry
    {
        std::shared_ptr<TaskRunner> task_runner;
        std::weak_ptr<FilePathWatcherImpl> watcher;
    };

    // There is no destructor because |g_inotify_reader| is a base::LazyInstace::Leaky object.
    // Having a destructor causes build issues with GCC 6 (http://crbug.com/636346).

    void startThread();

    std::mutex lock_;

    // Tracks which FilePathWatcherImpls to be notified on which watches. The tracked
    // FilePathWatcherImpl is keyed by raw pointers for fast look up and mapped to a WatchEntry that
    // is used to safely post a notification.
    std::unordered_map<Watch, std::map<FilePathWatcherImpl*, WatcherEntry>> watchers_;

    // File descriptor returned by inotify_init.
    const int inotify_fd_;

    // Thread delegate for the Inotify thread.
    InotifyReaderThreadDelegate thread_delegate_;

    // Flag set to true when startup was successful.
    bool valid_ = false;

    DISALLOW_COPY_AND_ASSIGN(InotifyReader);
};

class FilePathWatcherImpl
    : public std::enable_shared_from_this<FilePathWatcherImpl>,
      public FilePathWatcher::PlatformDelegate
{
public:
    explicit FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner);
    ~FilePathWatcherImpl() override;

    // Called for each event coming from the watch on the original thread. |fired_watch| identifies
    // the watch that fired, |child| indicates what has changed, and is relative to the currently
    // watched path for |fired_watch|.
    //
    // |created| is true if the object appears.
    // |deleted| is true if the object disappears.
    // |is_dir| is true if the object is a directory.
    void onFilePathChanged(InotifyReader::Watch fired_watch,
                           const std::filesystem::path::string_type& child,
                           bool created,
                           bool deleted,
                           bool is_dir);

    bool watch(const std::filesystem::path& path,
               bool recursive,
               const FilePathWatcher::Callback& callback) override;
    void cancel() override;

    // Returns whether the number of inotify watches of this FilePathWatcherImpl would exceed the
    // limit if adding one more.
    bool wouldExceedWatchLimit() const;

    // Inotify watches are installed for all directory components of |target_|.
    // A WatchEntry instance holds:
    // - |watch|: the watch descriptor for a component.
    // - |subdir|: the subdirectory that identifies the next component.
    //   - For the last component, there is no next component, so it is empty.
    // - |linkname|: the target of the symlink.
    //   - Only if the target being watched is a symbolic link.
    struct WatchEntry
    {
        explicit WatchEntry(const std::filesystem::path& dirname)
            : watch(InotifyReader::kInvalidWatch),
              subdir(dirname)
        {
            // Nothing
        }

        InotifyReader::Watch watch;
        std::filesystem::path::string_type subdir;
        std::filesystem::path::string_type linkname;
    };

private:
    // Reconfigure to watch for the most specific parent directory of |target_| that exists. Also
    // calls UpdateRecursiveWatches() below. Returns true if watch limit is not hit. Otherwise,
    // returns false.
    bool updateWatches();

    // Reconfigure to recursively watch |target_| and all its sub-directories.
    // - This is a no-op if the watch is not recursive.
    // - If |target_| does not exist, then clear all the recursive watches.
    // - Assuming |target_| exists, passing kInvalidWatch as |fired_watch| forces
    //   addition of recursive watches for |target_|.
    // - Otherwise, only the directory associated with |fired_watch| and its
    //   sub-directories will be reconfigured.
    // Returns true if watch limit is not hit. Otherwise, returns false.
    bool updateRecursiveWatches(InotifyReader::Watch fired_watch, bool is_dir);

    // Enumerate recursively through |path| and add / update watches.
    // Returns true if watch limit is not hit. Otherwise, returns false.
    bool updateRecursiveWatchesForPath(const std::filesystem::path& path);

    // Do internal bookkeeping to update mappings between |watch| and its
    // associated full path |path|.
    void trackWatchForRecursion(InotifyReader::Watch watch, const std::filesystem::path& path);

    // Remove all the recursive watches.
    void removeRecursiveWatches();

    // |path| is a symlink to a non-existent target. Attempt to add a watch to the link target's
    // parent directory. Update |watch_entry| on success.
    // Returns true if watch limit is not hit. Otherwise, returns false.
    bool addWatchForBrokenSymlink(const std::filesystem::path& path, WatchEntry* watch_entry);

    bool hasValidWatchVector() const;

    // Callback to notify upon changes.
    FilePathWatcher::Callback callback_;

    // Path we're supposed to watch (passed to callback).
    std::filesystem::path target_;

    // Set to true to watch the sub trees of the specified directory file path.
    bool recursive_watch_ = false;

    // The vector of watches and next component names for all path components, starting at the root
    // directory. The last entry corresponds to the watch for |target_| and always stores an empty
    // next component name in |subdir|.
    std::vector<WatchEntry> watches_;

    std::unordered_map<InotifyReader::Watch, std::filesystem::path> recursive_paths_by_watch_;
    std::map<std::filesystem::path, InotifyReader::Watch> recursive_watches_by_path_;
};

void InotifyReaderThreadDelegate::startReader()
{
    start(std::bind(&InotifyReaderThreadDelegate::run, this));
}

//--------------------------------------------------------------------------------------------------
void InotifyReaderThreadDelegate::run()
{
    std::array<pollfd, 1> fdarray{{{inotify_fd_, POLLIN, 0}}};

    while (!isStopping())
    {
        // Wait until some inotify events are available.
        int poll_result = HANDLE_EINTR(poll(fdarray.data(), fdarray.size(), -1));
        if (poll_result < 0)
        {
            DPLOG(LS_ERROR) << "poll failed";
            return;
        }

        // Adjust buffer size to current event queue size.
        int buffer_size;
        int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD, &buffer_size));
        if (ioctl_result != 0)
        {
            DPLOG(LS_ERROR) << "ioctl failed";
            return;
        }

        std::vector<char> buffer(buffer_size);
        ssize_t bytes_read = HANDLE_EINTR(read(inotify_fd_, &buffer[0], buffer_size));
        if (bytes_read < 0)
        {
            DPLOG(LS_ERROR) << "read from inotify fd failed";
            return;
        }

        ssize_t i = 0;
        while (i < bytes_read)
        {
            inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
            size_t event_size = sizeof(inotify_event) + event->len;
            DCHECK(i + event_size <= static_cast<size_t>(bytes_read));
            InotifyReader::instance().onInotifyEvent(event);
            i += event_size;
        }
    }
}

//--------------------------------------------------------------------------------------------------
InotifyReader::InotifyReader()
    : inotify_fd_(inotify_init()),
    thread_delegate_(inotify_fd_)
{
    if (inotify_fd_ < 0)
    {
        PLOG(LS_ERROR) << "inotify_init() failed";
        return;
    }

    startThread();
    valid_ = true;
}

//--------------------------------------------------------------------------------------------------
// static
InotifyReader& InotifyReader::instance()
{
    static InotifyReader inotifyReader;
    return inotifyReader;
}

//--------------------------------------------------------------------------------------------------
InotifyReader::Watch InotifyReader::addWatch(
    const std::filesystem::path& path, FilePathWatcherImpl* watcher)
{
    if (!valid_)
        return kInvalidWatch;

    if (watcher->wouldExceedWatchLimit())
        return kWatchLimitExceeded;

    std::lock_guard auto_lock(lock_);

    Watch watch = inotify_add_watch(inotify_fd_, path.c_str(),
                                    IN_ATTRIB | IN_CREATE | IN_DELETE |
                                        IN_CLOSE_WRITE | IN_MOVE |
                                        IN_ONLYDIR);

    if (watch == kInvalidWatch)
        return kInvalidWatch;

    watchers_[watch].emplace(std::make_pair(
        watcher, WatcherEntry{watcher->taskRunner(), watcher->weak_from_this()}));

    return watch;
}

//--------------------------------------------------------------------------------------------------
void InotifyReader::removeWatch(Watch watch, FilePathWatcherImpl* watcher)
{
    if (!valid_ || (watch == kInvalidWatch))
        return;

    std::lock_guard auto_lock(lock_);

    auto watchers_it = watchers_.find(watch);
    if (watchers_it == watchers_.end())
        return;

    auto& watcher_map = watchers_it->second;
    watcher_map.erase(watcher);

    if (watcher_map.empty())
    {
        watchers_.erase(watchers_it);
        inotify_rm_watch(inotify_fd_, watch);
    }
}

//--------------------------------------------------------------------------------------------------
void InotifyReader::onInotifyEvent(const inotify_event* event)
{
    if (event->mask & IN_IGNORED)
        return;

    std::filesystem::path::string_type child(event->len ? event->name : "");
    std::lock_guard auto_lock(lock_);

    // In racing conditions, RemoveWatch() could grab `lock_` first and remove the entry for `event->wd`.
    auto watchers_it = watchers_.find(event->wd);
    if (watchers_it == watchers_.end())
        return;

    auto& watcher_map = watchers_it->second;
    for (const auto& entry : watcher_map)
    {
        auto& watcher_entry = entry.second;
        std::shared_ptr<FilePathWatcherImpl> watcher = watcher_entry.watcher.lock();

        auto task = std::bind(&FilePathWatcherImpl::onFilePathChanged,
                              watcher,
                              event->wd,
                              child,
                              event->mask & (IN_CREATE | IN_MOVED_TO),
                              event->mask & (IN_DELETE | IN_MOVED_FROM),
                              event->mask & IN_ISDIR);
        watcher_entry.task_runner->postTask(std::move(task));
    }
}

//--------------------------------------------------------------------------------------------------
bool InotifyReader::hasWatches()
{
    std::lock_guard auto_lock(lock_);
    return !watchers_.empty();
}

//--------------------------------------------------------------------------------------------------
void InotifyReader::startThread()
{
    // This object is LazyInstance::Leaky, so thread_delegate_ will outlive the thread.
    thread_delegate_.startReader();
}

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::FilePathWatcherImpl(std::shared_ptr<TaskRunner> task_runner)
    : FilePathWatcher::PlatformDelegate(task_runner)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FilePathWatcherImpl::~FilePathWatcherImpl()
{
    DCHECK(!taskRunner() || taskRunner()->belongsToCurrentThread());
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::onFilePathChanged(InotifyReader::Watch fired_watch,
                                            const std::filesystem::path::string_type& child,
                                            bool created,
                                            bool deleted,
                                            bool is_dir)
{
    DCHECK(!watches_.empty());
    DCHECK(hasValidWatchVector());

    // Used below to avoid multiple recursive updates.
    bool did_update = false;

    // Whether kWatchLimitExceeded is encountered during update.
    bool exceeded_limit = false;

    // Find the entries in |watches_| that correspond to |fired_watch|.
    for (size_t i = 0; i < watches_.size(); ++i)
    {
        const WatchEntry& watch_entry = watches_[i];
        if (fired_watch != watch_entry.watch)
            continue;

        // Check whether a path component of |target_| changed.
        bool change_on_target_path =
            child.empty() || (child == watch_entry.linkname) || (child == watch_entry.subdir);

        // Check if the change references |target_| or a direct child of |target_|.
        bool target_changed;
        if (watch_entry.subdir.empty())
        {
            // The fired watch is for a WatchEntry without a subdir. Thus for a given
            // |target_| = "/path/to/foo", this is for "foo". Here, check either:
            // - the target has no symlink: it is the target and it changed.
            // - the target has a symlink, and it matches |child|.
            target_changed = (watch_entry.linkname.empty() || child == watch_entry.linkname);
        }
        else
        {
            // The fired watch is for a WatchEntry with a subdir. Thus for a given
            // |target_| = "/path/to/foo", this is for {"/", "/path", "/path/to"}.
            // So we can safely access the next WatchEntry since we have not reached
            // the end yet. Check |watch_entry| is for "/path/to", i.e. the next
            // element is "foo".
            bool next_watch_may_be_for_target = watches_[i + 1].subdir.empty();
            if (next_watch_may_be_for_target)
            {
                // The current |watch_entry| is for "/path/to", so check if the |child|
                // that changed is "foo".
                target_changed = watch_entry.subdir == child;
            }
            else
            {
                // The current |watch_entry| is not for "/path/to", so the next entry
                // cannot be "foo". Thus |target_| has not changed.
                target_changed = false;
            }
        }

        // Update watches if a directory component of the |target_| path (dis)appears. Note that we
        // don't add the additional restriction of checking the event mask to see if it is for a
        // directory here as changes to symlinks on the target path will not have IN_ISDIR set in
        // the event masks. As a result we may sometimes call UpdateWatches() unnecessarily.
        if (change_on_target_path && (created || deleted) && !did_update)
        {
            if (!updateWatches())
            {
                exceeded_limit = true;
                break;
            }
            did_update = true;
        }

        // Report the following events:
        //  - The target or a direct child of the target got changed (in case the
        //    watched path refers to a directory).
        //  - One of the parent directories got moved or deleted, since the target
        //    disappears in this case.
        //  - One of the parent directories appears. The event corresponding to
        //    the target appearing might have been missed in this case, so recheck.
        if (target_changed ||
            (change_on_target_path && deleted) ||
            (change_on_target_path && created && isPathExists(target_)))
        {
            if (!did_update)
            {
                if (!updateRecursiveWatches(fired_watch, is_dir))
                {
                    exceeded_limit = true;
                    break;
                }
                did_update = true;
            }
            callback_(target_, /*error=*/false);  // `this` may be deleted.
            return;
        }
    }

    if (!exceeded_limit && contains(recursive_paths_by_watch_, fired_watch))
    {
        if (!did_update)
        {
            if (!updateRecursiveWatches(fired_watch, is_dir))
                exceeded_limit = true;
        }
        if (!exceeded_limit)
        {
            callback_(target_, /*error=*/false);  // `this` may be deleted.
            return;
        }
    }

    if (exceeded_limit)
    {
        // Reset states and cancels all watches.
        auto callback = callback_;
        cancel();

        // Fires the "error=true" callback.
        callback(target_, /*error=*/true);  // `this` may be deleted.
    }
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::watch(const std::filesystem::path& path,
                                bool recursive,
                                const FilePathWatcher::Callback& callback)
{
    DCHECK(target_.empty());

    callback_ = callback;
    target_ = path;
    recursive_watch_ = recursive;

    for (auto it = target_.begin(); it != target_.end(); ++it)
        watches_.emplace_back(it->native());

    watches_.emplace_back(std::filesystem::path::string_type());

    if (!updateWatches())
    {
        cancel();
        // Note `callback` is not invoked since false is returned.
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::cancel()
{
    if (!callback_)
    {
        // watch() was never called.
        setCancelled();
        return;
    }

    DCHECK(taskRunner()->belongsToCurrentThread());
    DCHECK(!isCancelled());

    setCancelled();
    callback_ = nullptr;

    for (const auto& watch : watches_)
        InotifyReader::instance().removeWatch(watch.watch, this);
    watches_.clear();
    target_.clear();
    removeRecursiveWatches();
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::wouldExceedWatchLimit() const
{
    // `watches_` contains inotify watches of all dir components of `target_`.
    // `recursive_paths_by_watch_` contains inotify watches for sub dirs under
    // `target_` of a Type::kRecursive watcher and keyed by inotify watches.
    // All inotify watches used by this FilePathWatcherImpl are either in
    // `watches_` or as a key in `recursive_paths_by_watch_`. As a result, the
    // two provide a good estimate on the number of inofiy watches used by this
    // FilePathWatcherImpl.
    const size_t number_of_inotify_watches =
        watches_.size() + recursive_paths_by_watch_.size();
    return number_of_inotify_watches >= maxNumberOfInotifyWatches();
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::updateWatches()
{
    // Ensure this runs on the task_runner() exclusively in order to avoid concurrency issues.
    DCHECK(taskRunner()->belongsToCurrentThread());
    DCHECK(hasValidWatchVector());

    // Walk the list of watches and update them as we go.
    std::filesystem::path path("/");
    for (WatchEntry& watch_entry : watches_)
    {
        InotifyReader::Watch old_watch = watch_entry.watch;
        watch_entry.watch = InotifyReader::kInvalidWatch;
        watch_entry.linkname.clear();
        watch_entry.watch = InotifyReader::instance().addWatch(path, this);

        if (watch_entry.watch == InotifyReader::kWatchLimitExceeded)
            return false;

        if (watch_entry.watch == InotifyReader::kInvalidWatch)
        {
            // Ignore the error code (beyond symlink handling) to attempt to add
            // watches on accessible children of unreadable directories. Note that
            // this is a best-effort attempt; we may not catch events in this
            // scenario.
            if (isLink(path))
            {
                if (!addWatchForBrokenSymlink(path, &watch_entry))
                    return false;
            }
        }

        if (old_watch != watch_entry.watch)
            InotifyReader::instance().removeWatch(old_watch, this);

        path = path.append(watch_entry.subdir);
    }

    return updateRecursiveWatches(InotifyReader::kInvalidWatch, /*is_dir=*/false);
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::updateRecursiveWatches(InotifyReader::Watch fired_watch, bool is_dir)
{
    DCHECK(hasValidWatchVector());

    if (!recursive_watch_)
        return true;

    if (!isPathExists(target_))
    {
        removeRecursiveWatches();
        return true;
    }

    // Check to see if this is a forced update or if some component of |target_| has changed. For
    // these cases, redo the watches for |target_| and below.
    if (!contains(recursive_paths_by_watch_, fired_watch) && fired_watch != watches_.back().watch)
    {
        return updateRecursiveWatchesForPath(target_);
    }

    // Underneath |target_|, only directory changes trigger watch updates.
    if (!is_dir)
        return true;

    const std::filesystem::path& changed_dir = contains(recursive_paths_by_watch_, fired_watch)
        ? recursive_paths_by_watch_[fired_watch] : target_;

    auto start_it = recursive_watches_by_path_.lower_bound(changed_dir);
    auto end_it = start_it;

    for (; end_it != recursive_watches_by_path_.end(); ++end_it)
    {
        const std::filesystem::path& cur_path = end_it->first;
        if (changed_dir != cur_path.parent_path())
            break;

        // There could be a race when another process is changing contents under `changed_dir` while
        // chrome is watching (e.g. an Android app updating a dir with Chrome OS file manager open
        // for the dir). In such case, `cur_dir` under `changed_dir` could exist in this loop but
        // not in the FileEnumerator loop in the upcoming UpdateRecursiveWatchesForPath(), As a
        // result, `g_inotify_reader` would have an entry in its `watchers_` pointing to `this` but
        // `this` is no longer aware of that. Crash in http://crbug/990004 could happen later.
        //
        // Remove the watcher of `cur_path` regardless of whether it exists or not to keep `this`
        // and `g_inotify_reader` consistent even when the race happens. The watcher will be added
        // back if `cur_path` exists in the FileEnumerator loop in UpdateRecursiveWatchesForPath().
        InotifyReader::instance().removeWatch(end_it->second, this);

        // Keep it in sync with |recursive_watches_by_path_| crbug.com/995196.
        recursive_paths_by_watch_.erase(end_it->second);
    }

    recursive_watches_by_path_.erase(start_it, end_it);
    return updateRecursiveWatchesForPath(changed_dir);
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::updateRecursiveWatchesForPath(const std::filesystem::path& path)
{
    // Note: SHOW_SYM_LINKS exposes symlinks as symlinks, so they are ignored
    // rather than followed. Following symlinks can easily lead to the undesirable
    // situation where the entire file system is being watched.
    std::error_code ignored_error;
    std::filesystem::recursive_directory_iterator iterator(
        path, std::filesystem::directory_options::follow_directory_symlink, ignored_error);

    for (const std::filesystem::directory_entry& current : iterator)
    {
        if (!current.is_directory())
            continue;

        if (!contains(recursive_watches_by_path_, current))
        {
            // Add new watches.
            InotifyReader::Watch watch = InotifyReader::instance().addWatch(current, this);
            if (watch == InotifyReader::kWatchLimitExceeded)
                return false;

            trackWatchForRecursion(watch, current);
        }
        else
        {
            // Update existing watches.
            InotifyReader::Watch old_watch = recursive_watches_by_path_[current];
            DCHECK_NE(InotifyReader::kInvalidWatch, old_watch);
            InotifyReader::Watch watch = InotifyReader::instance().addWatch(current, this);

            if (watch == InotifyReader::kWatchLimitExceeded)
                return false;

            if (watch != old_watch)
            {
                InotifyReader::instance().removeWatch(old_watch, this);
                recursive_paths_by_watch_.erase(old_watch);
                recursive_watches_by_path_.erase(current);
                trackWatchForRecursion(watch, current);
            }
        }
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::trackWatchForRecursion(InotifyReader::Watch watch,
                                                 const std::filesystem::path& path)
{
    if (watch == InotifyReader::kInvalidWatch)
        return;

    recursive_paths_by_watch_[watch] = path;
    recursive_watches_by_path_[path] = watch;
}

//--------------------------------------------------------------------------------------------------
void FilePathWatcherImpl::removeRecursiveWatches()
{
    if (!recursive_watch_)
        return;

    for (const auto& it : recursive_paths_by_watch_)
        InotifyReader::instance().removeWatch(it.first, this);

    recursive_paths_by_watch_.clear();
    recursive_watches_by_path_.clear();
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::addWatchForBrokenSymlink(const std::filesystem::path& path,
                                                   WatchEntry* watch_entry)
{
    DCHECK_EQ(InotifyReader::kInvalidWatch, watch_entry->watch);
    std::filesystem::path link;
    if (!readSymbolicLink(path, &link))
        return true;

    if (!link.is_absolute())
    {
        std::filesystem::path temp(path);
        temp.remove_filename();
        temp += link;
        link = temp;
    }

    std::filesystem::path dir_name(link);
    dir_name.remove_filename();

    // Try watching symlink target directory. If the link target is "/", then we shouldn't get here
    // in normal situations and if we do, we'd watch "/" for changes to a component "/" which is
    // harmless so no special treatment of this case is required.

    InotifyReader::Watch watch = InotifyReader::instance().addWatch(dir_name, this);
    if (watch == InotifyReader::kWatchLimitExceeded)
        return false;
    if (watch == InotifyReader::kInvalidWatch)
    {
        // TODO(craig) Symlinks only work if the parent directory for the target exist. Ideally we
        // should make sure we've watched all the components of the symlink path for changes.
        // See crbug.com/91561 for details.
        DPLOG(LS_ERROR) << "Watch failed for "  << dir_name;
        return true;
    }
    watch_entry->watch = watch;
    watch_entry->linkname = link.parent_path().native();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcherImpl::hasValidWatchVector() const
{
    if (watches_.empty())
        return false;

    for (size_t i = 0; i < watches_.size() - 1; ++i)
    {
        if (watches_[i].subdir.empty())
            return false;
    }

    return watches_.back().subdir.empty();
}

} // namespace

//--------------------------------------------------------------------------------------------------
FilePathWatcher::FilePathWatcher(std::shared_ptr<TaskRunner> task_runner)
{
    impl_ = std::make_shared<FilePathWatcherImpl>(std::move(task_runner));
}

} // namespace base
