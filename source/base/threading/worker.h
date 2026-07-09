//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_THREADING_WORKER_H
#define BASE_THREADING_WORKER_H

#include <QObject>

#include "base/threading/thread.h"

class WorkerManager;

class Worker : public QObject
{
    Q_OBJECT

public:
    Worker();
    ~Worker() override;

protected:
    friend class WorkerManager;

    void start(WorkerManager* manager);
    void stopSoon();

    // Runs inside the worker thread before its event loop starts. Create here every
    // object that must live in the worker thread.
    virtual void onStart() = 0;

    // Runs inside the worker thread after its event loop ends. Destroy here everything
    // created in onStart().
    virtual void onStop() = 0;

private slots:
    void onThreadStarted();
    void onThreadFinished();

private:
    WorkerManager* manager_ = nullptr;
    Thread thread_;

    Q_DISABLE_COPY_MOVE(Worker)
};

#endif // BASE_THREADING_WORKER_H
