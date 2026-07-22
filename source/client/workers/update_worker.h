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

#ifndef CLIENT_WORKERS_UPDATE_WORKER_H
#define CLIENT_WORKERS_UPDATE_WORKER_H

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "common/update_info.h"

class UpdateChecker;

class UpdateWorker final : public Worker
{
    Q_OBJECT

public:
    UpdateWorker();
    ~UpdateWorker() final;

signals:
    // Emitted from the worker thread when the update server has a newer version than the
    // running one.
    void sig_updateAvailable(const UpdateInfo& update_info);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onUpdateCheckedFinished(const QByteArray& result);

private:
    ScopedQPointer<UpdateChecker> update_checker_;

    Q_DISABLE_COPY_MOVE(UpdateWorker)
};

#endif // CLIENT_WORKERS_UPDATE_WORKER_H
