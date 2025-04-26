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

#ifndef BASE_QT_TASK_RUNNER_H
#define BASE_QT_TASK_RUNNER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"

namespace base {

class QtTaskRunner final : public base::TaskRunner
{
public:
    QtTaskRunner();
    ~QtTaskRunner() final;

    // TaskRunner implementation.
    bool belongsToCurrentThread() const final;
    void postTask(Callback callback) final;
    void postDelayedTask(Callback callback, const Milliseconds& delay) final;
    void postNonNestableTask(Callback callback) final;
    void postNonNestableDelayedTask(Callback callback, const Milliseconds& delay) final;
    void postQuit() final;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(QtTaskRunner);
};

} // namespace base

#endif // BASE_QT_TASK_RUNNER_H
