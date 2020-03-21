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

#ifndef QT_BASE__QT_TASK_RUNNER_H
#define QT_BASE__QT_TASK_RUNNER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"

namespace qt_base {

class QtTaskRunner : public base::TaskRunner
{
public:
    QtTaskRunner();
    ~QtTaskRunner();

    // TaskRunner implementation.
    bool belongsToCurrentThread() const override;
    void postTask(Callback callback) override;
    void postDelayedTask(Callback callback, Milliseconds delay) override;
    void postNonNestableTask(Callback callback) override;
    void postNonNestableDelayedTask(Callback callback, Milliseconds delay) override;
    void postQuit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(QtTaskRunner);
};

} // namespace qt_base

#endif // QT_BASE__QT_TASK_RUNNER_H
