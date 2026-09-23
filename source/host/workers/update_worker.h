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

#ifndef HOST_WORKERS_UPDATE_WORKER_H
#define HOST_WORKERS_UPDATE_WORKER_H

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "base/update/update_info.h"

class HttpFileDownloader;
class UpdateChecker;
class UpdateInstaller;

class UpdateWorker final : public Worker
{
    Q_OBJECT

public:
    UpdateWorker();
    ~UpdateWorker() final;

public slots:
    // Starts an update check unless one is already in progress.
    void onCheckUpdates();

signals:
    void sig_updateCheckStarted();

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onUpdateCheckFinished(const UpdateInfo& update_info);
    void onUpdateCheckFailed();
    void onFileDownloaderError(const QString& error);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);

private:
    void checkForUpdates();

    ScopedQPointer<UpdateChecker> update_checker_;
    ScopedQPointer<HttpFileDownloader> update_downloader_;
    ScopedQPointer<UpdateInstaller> update_installer_;

    Q_DISABLE_COPY_MOVE(UpdateWorker)
};

#endif // HOST_WORKERS_UPDATE_WORKER_H
