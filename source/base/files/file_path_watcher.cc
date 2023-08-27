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
#include "build/build_config.h"

namespace base {

//--------------------------------------------------------------------------------------------------
FilePathWatcher::~FilePathWatcher()
{
    impl_->cancel();
}

//--------------------------------------------------------------------------------------------------
// static
bool FilePathWatcher::recursiveWatchAvailable()
{
#if (defined(OS_MAC) && !defined(OS_IOS)) || defined(OS_WIN) || \
    defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_AIX)
    return true;
#else
    // FSEvents isn't available on iOS.
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
FilePathWatcher::PlatformDelegate::PlatformDelegate(std::shared_ptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      cancelled_(false)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FilePathWatcher::PlatformDelegate::~PlatformDelegate()
{
    DCHECK(isCancelled());
}

//--------------------------------------------------------------------------------------------------
bool FilePathWatcher::watch(const std::filesystem::path& path,
                            bool recursive,
                            const Callback& callback)
{
    DCHECK(path.is_absolute());
    return impl_->watch(path, recursive, callback);
}

} // namespace base
