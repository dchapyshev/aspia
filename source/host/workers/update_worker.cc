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
#include "base/version_constants.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "common/update_info.h"

#if defined(Q_OS_WINDOWS)
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <ctime>

#include "base/process_util.h"
#include "base/crypto/random.h"
#include "base/files/file_util.h"
#include "host/host_storage.h"
#include "host/system_settings.h"
#endif // defined(Q_OS_WINDOWS)

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
#if defined(Q_OS_WINDOWS)
    if (update_checker_)
    {
        LOG(INFO) << "Update check already in progress";
        return;
    }

    update_checker_ = new UpdateChecker(SystemSettings().updateServer(), "host", this);

    connect(update_checker_, &UpdateChecker::sig_checkedFinished,
            this, &UpdateWorker::onUpdateCheckedFinished);

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStart()
{
    LOG(INFO) << "Update worker started";
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
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onTimer(TimePoint /* now */)
{
    checkForUpdates();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onUpdateCheckedFinished(const QByteArray& result)
{
    CHECK(update_checker_);

    do
    {
        if (result.isEmpty())
        {
            LOG(ERROR) << "Error while retrieving update information";
            break;
        }

        UpdateInfo update_info = UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
            break;
        }

        const QVersionNumber& current_version = kCurrentVersion;
        const QVersionNumber& update_version = update_info.version();

        if (update_version <= current_version)
        {
            LOG(INFO) << "No available updates";
            break;
        }

        LOG(INFO) << "New version available:" << update_version.toString();

        update_downloader_ = new HttpFileDownloader(update_info.url(), this);

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
void UpdateWorker::onFileDownloaderError(int error_code)
{
    LOG(ERROR) << "Unable to download update:" << error_code;
    CHECK(update_downloader_);

    update_downloader_->disconnect(this);
    update_downloader_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onFileDownloaderCompleted()
{
    CHECK(update_downloader_);

#if defined(Q_OS_WINDOWS)
    do
    {
        QString file_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (file_path.isEmpty())
        {
            LOG(ERROR) << "Unable to get temp directory";
            break;
        }

        QDir().mkpath(file_path);

        QString file_name =
            "/aspia_host_" + QString::fromLatin1(Random::byteArray(16).toHex()) + ".msi";

        file_path = QDir::toNativeSeparators(file_path.append(file_name));

        if (!writeFile(file_path, update_downloader_->data()))
        {
            LOG(ERROR) << "Unable to write file" << file_path;
            break;
        }

        QString arguments;

        arguments += "/i "; // Normal install.
        arguments += file_path; // MSI package file.
        arguments += " /qn"; // No UI during the installation process.

        if (!ProcessUtil::createProcess("msiexec", arguments, ProcessUtil::ExecuteMode::ELEVATE))
        {
            LOG(ERROR) << "Unable to create update process (cmd:" << arguments << ")";

            // If the update fails, delete the temporary file.
            if (!QFile::remove(file_path))
                LOG(ERROR) << "Unable to remove installer file";
            break;
        }

        LOG(INFO) << "Update process started (cmd:" << arguments << ")";
    }
    while (false);
#endif // defined(Q_OS_WINDOWS)

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
