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

#include "base/update/console_updater.h"

#include <QEventLoop>
#include <QTextStream>

#include "base/build_config.h"
#include "base/process_util.h"
#include "base/version_constants.h"
#include "base/net/http_file_downloader.h"
#include "base/update/update_checker.h"
#include "base/update/update_installer.h"

namespace {

// Percent. The progress of a download is reported in steps of this size, so that a slow download
// does not fill the terminal with lines nobody reads.
const int kProgressStep = 10;

//--------------------------------------------------------------------------------------------------
bool isKnownChannel(const QString& channel)
{
    return channel == kStableUpdateChannel || channel == kBetaUpdateChannel ||
           channel == kAlphaUpdateChannel;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ConsoleUpdater::ConsoleUpdater(const QString& package, QTextStream& out)
    : package_(package),
      out_(out)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ConsoleUpdater::~ConsoleUpdater() = default;

//--------------------------------------------------------------------------------------------------
int ConsoleUpdater::check(const QString& channel)
{
    int exit_code = 0;
    offeredUpdate(channel, &exit_code);
    return exit_code;
}

//--------------------------------------------------------------------------------------------------
int ConsoleUpdater::install(const QString& channel)
{
    if (!ProcessUtil::isPrivileged())
    {
        out_ << "Not enough rights to install the update." << Qt::endl;
        return 1;
    }

    int exit_code = 0;

    std::optional<UpdateInfo> update_info = offeredUpdate(channel, &exit_code);
    if (!update_info)
        return exit_code;

    if (!UpdateInstaller::isSupported(update_info->format()))
    {
        out_ << "Packages of format " << update_info->format() << " are not installed by this "
             << "application. Install the update yourself: " << update_info->url() << Qt::endl;
        return 1;
    }

    UpdateInstaller installer(UpdateInstaller::Mode::USER);

    QString file_path = installer.createPackageFile(*update_info);
    if (file_path.isEmpty())
    {
        out_ << "Unable to create a file for the package." << Qt::endl;
        return 1;
    }

    QString error;
    if (!download(*update_info, file_path, &error))
    {
        out_ << "Unable to download the update: " << error << Qt::endl;
        return 1;
    }

    out_ << "Installing the update..." << Qt::endl;

    bool success = false;
    QEventLoop loop;

    QObject::connect(&installer, &UpdateInstaller::sig_finished, &loop,
                     [&](bool installer_success, const QString& installer_error)
    {
        success = installer_success;
        error = installer_error;
        loop.quit();
    });

    UpdateInstaller::Result result = installer.install();

    if (result == UpdateInstaller::Result::DAMAGED)
    {
        out_ << "The downloaded file does not match the manifest." << Qt::endl;
        return 1;
    }

    if (result == UpdateInstaller::Result::FAILED)
    {
        out_ << "Unable to start the installation of the update." << Qt::endl;
        return 1;
    }

    loop.exec();

    if (!success)
    {
        out_ << "The installation failed. " << error << Qt::endl;
        return 1;
    }

#if defined(Q_OS_WINDOWS)
    // The installer of the system does the installation in a process of its own, and this one is
    // gone before it has anything to say about how it went.
    out_ << "The installation is started." << Qt::endl;
#else
    out_ << "The update is installed." << Qt::endl;
#endif // defined(Q_OS_WINDOWS)

    return 0;
}

//--------------------------------------------------------------------------------------------------
std::optional<UpdateInfo> ConsoleUpdater::checkForUpdates(const QString& channel)
{
    out_ << "Checking for updates..." << Qt::endl;

    UpdateChecker checker(channel, package_);
    std::optional<UpdateInfo> result;
    QEventLoop loop;

    QObject::connect(&checker, &UpdateChecker::sig_checkFinished, &loop,
                     [&](const UpdateInfo& update_info)
    {
        result = update_info;
        loop.quit();
    });

    QObject::connect(&checker, &UpdateChecker::sig_checkFailed, &loop, [&]()
    {
        loop.quit();
    });

    checker.start();
    loop.exec();

    return result;
}

//--------------------------------------------------------------------------------------------------
bool ConsoleUpdater::download(const UpdateInfo& update_info, const QString& file_path, QString* error)
{
    out_ << "Downloading " << update_info.url() << Qt::endl;

    HttpFileDownloader downloader(update_info.url(), file_path);
    bool completed = false;
    int reported = 0;
    QEventLoop loop;

    QObject::connect(&downloader, &HttpFileDownloader::sig_downloadProgress, &loop, [&](int percentage)
    {
        if (percentage < reported + kProgressStep)
            return;

        reported = percentage - (percentage % kProgressStep);
        out_ << reported << "%" << Qt::endl;
    });

    QObject::connect(&downloader, &HttpFileDownloader::sig_downloadCompleted, &loop, [&]()
    {
        completed = true;
        loop.quit();
    });

    QObject::connect(&downloader, &HttpFileDownloader::sig_downloadError, &loop,
                     [&](const QString& downloader_error)
    {
        *error = downloader_error;
        loop.quit();
    });

    downloader.start();
    loop.exec();

    return completed;
}

//--------------------------------------------------------------------------------------------------
std::optional<UpdateInfo> ConsoleUpdater::offeredUpdate(const QString& channel, int* exit_code)
{
    if (!isKnownChannel(channel))
    {
        out_ << "Unknown update channel: " << channel << Qt::endl;
        *exit_code = 1;
        return std::nullopt;
    }

    out_ << "Installed version: " << kCurrentVersion.toString() << Qt::endl;

    std::optional<UpdateInfo> update_info = checkForUpdates(channel);
    if (!update_info)
    {
        out_ << "Error retrieving update information." << Qt::endl;
        *exit_code = 1;
        return std::nullopt;
    }

    if (!update_info->isValid())
    {
        out_ << "No updates available." << Qt::endl;
        return std::nullopt;
    }

    out_ << "Version " << update_info->version().toString() << " is available." << Qt::endl;

    QString description = update_info->description();
    if (!description.isEmpty())
        out_ << description << Qt::endl;

    return update_info;
}
