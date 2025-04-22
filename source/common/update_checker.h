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

#ifndef COMMON_UPDATE_CHECKER_H
#define COMMON_UPDATE_CHECKER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/threading/simple_thread.h"

#include <QByteArray>
#include <QString>

namespace common {

class UpdateChecker
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void onUpdateCheckedFinished(const QByteArray& result) = 0;
    };

    UpdateChecker();
    ~UpdateChecker();

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

    void start(std::shared_ptr<base::TaskRunner> owner_task_runner, Delegate* delegate);

private:
    void run();

    class Runner;
    std::shared_ptr<Runner> runner_;
    base::SimpleThread thread_;
    QString update_server_;
    QString package_name_;

    DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

} // namespace common

#endif // COMMON_UPDATE_CHECKER_H
