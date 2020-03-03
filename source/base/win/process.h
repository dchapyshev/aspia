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

#ifndef BASE__WIN__PROCESS_H
#define BASE__WIN__PROCESS_H

#include "base/process_handle.h"
#include "base/session_id.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_object.h"

#include <filesystem>
#include <functional>

namespace base::win {

class Process : public ObjectWatcher::Delegate
{
public:
    Process(std::shared_ptr<TaskRunner> task_runner, ProcessId process_id);
    Process(std::shared_ptr<TaskRunner> task_runner, HANDLE process, HANDLE thread);
    ~Process();

    using ExitCallback = std::function<void(int exit_code)>;

    void startWatching(const ExitCallback& callback);
    void stopWatching();

    bool isValid() const;

    std::filesystem::path filePath() const;
    std::u16string fileName() const;
    ProcessId processId() const;
    SessionId sessionId() const;

    int exitCode() const;

    void kill();
    void terminate();

    HANDLE native() const { return process_.get(); }

protected:
    // ObjectWatcher::Delegate implementation.
    void onObjectSignaled(HANDLE object) override;

private:
    ObjectWatcher watcher_;
    ExitCallback callback_;

    ScopedHandle process_;
    ScopedHandle thread_;

    DISALLOW_COPY_AND_ASSIGN(Process);
};

} // namespace base::win

#endif // BASE__WIN__PROCESS_H
