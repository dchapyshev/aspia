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

#include "host/workers/update_worker.h"

#include "base/logging.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "common/update_info.h"

#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
#include <ctime>

#include "common/update_installer.h"
#include "host/host_storage.h"
#include "host/system_settings.h"
#endif // defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
UpdateWorker::UpdateWorker()
    : Worker(Thread::AsioDispatcher, Seconds(30))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UpdateWorker::~UpdateWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onCheckUpdates()
{
#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    if (update_checker_)
    {
        LOG(INFO) << "Update check already in progress";
        return;
    }

    update_checker_ = new UpdateChecker(SystemSettings().updateServer(), "host", this);

    connect(update_checker_, &UpdateChecker::sig_checkFinished,
            this, &UpdateWorker::onUpdateCheckFinished);
    connect(update_checker_, &UpdateChecker::sig_checkFailed,
            this, &UpdateWorker::onUpdateCheckFailed);

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStop()
{
    LOG(INFO) << "Update worker stopped";

    if (update_checker_)
    {
        update_checker_->disconnect(this);
        update_checker_.reset();
    }

    if (update_downloader_)
    {
        update_downloader_->disconnect(this);
        update_downloader_.reset();
    }

    update_installer_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onTimer(TimePoint /* now */)
{
    checkForUpdates();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onUpdateCheckFinished(const UpdateInfo& update_info)
{
    CHECK(update_checker_);

    do
    {
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
            break;
        }

        LOG(INFO) << "New version available:" << update_info.version().toString();

        update_installer_ = new UpdateInstaller(UpdateInstaller::Mode::SERVICE, this);

        QString file_path = update_installer_->createPackageFile(update_info);
        if (file_path.isEmpty())
        {
            update_installer_.reset();
            break;
        }

        update_downloader_ = new HttpFileDownloader(update_info.url(), file_path, this);

        connect(update_downloader_, &HttpFileDownloader::sig_downloadError,
                this, &UpdateWorker::onFileDownloaderError);
        connect(update_downloader_, &HttpFileDownloader::sig_downloadCompleted,
                this, &UpdateWorker::onFileDownloaderCompleted);
        connect(update_downloader_, &HttpFileDownloader::sig_downloadProgress,
                this, &UpdateWorker::onFileDownloaderProgress);

        update_downloader_->start();
    }
    while (false);

    update_checker_->disconnect(this);
    update_checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onUpdateCheckFailed()
{
    CHECK(update_checker_);

    LOG(ERROR) << "Error while retrieving update information";

    update_checker_->disconnect(this);
    update_checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onFileDownloaderError(const QString& error)
{
    LOG(ERROR) << "Unable to download update:" << error;
    CHECK(update_downloader_);

    update_downloader_->disconnect(this);
    update_downloader_.reset();
    update_installer_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onFileDownloaderCompleted()
{
    CHECK(update_downloader_);

#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    CHECK(update_installer_);

    // Nothing waits for the installer here. The package restarts the service that started it.
    update_installer_->install();
    update_installer_.reset();
#endif // defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)

    update_downloader_->disconnect(this);
    update_downloader_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onFileDownloaderProgress(int percentage)
{
    LOG(INFO) << "Update downloading progress:" << percentage << "%";
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::checkForUpdates()
{
#if defined(Q_OS_WINDOWS)
    SystemSettings settings;
    if (!settings.isAutoUpdateEnabled())
        return;

    HostStorage storage;

    qint64 last_timepoint = storage.lastUpdateCheck();
    qint64 current_timepoint = std::time(nullptr);

    qint64 time_diff = current_timepoint - last_timepoint;
    if (time_diff <= 0)
    {
        storage.setLastUpdateCheck(current_timepoint);
        return;
    }

    static const qint64 kSecondsPerMinute = 60;
    static const qint64 kMinutesPerHour = 60;
    static const qint64 kHoursPerDay = 24;

    qint64 days = time_diff / kSecondsPerMinute / kMinutesPerHour / kHoursPerDay;
    if (days < 1)
        return;

    if (days < settings.updateCheckFrequency())
        return;

    storage.setLastUpdateCheck(current_timepoint);

    onCheckUpdates();
#endif // defined(Q_OS_WINDOWS)
}
