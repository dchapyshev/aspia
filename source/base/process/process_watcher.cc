//
// PROJECT:         Aspia
// FILE:            base/process/process_watcher.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_watcher.h"
#include "base/logging.h"

namespace aspia {

ProcessWatcher::~ProcessWatcher()
{
    StopWatching();
}

bool ProcessWatcher::StartWatching(const Process& process,
                                   EventCallback callback)
{
    DCHECK(process_handle_ == kNullProcessHandle);

    process_handle_ = process.Handle();
    callback_ = std::move(callback);
    return watcher_.StartWatching(process_handle_, this);
}

void ProcessWatcher::StopWatching()
{
    watcher_.StopWatching();
    process_handle_ = kNullProcessHandle;
    callback_ = nullptr;
}

void ProcessWatcher::OnObjectSignaled(HANDLE object)
{
    DCHECK(process_handle_ == object);
    process_handle_ = kNullProcessHandle;
    callback_();
}

} // namespace aspia
