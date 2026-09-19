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

#ifndef COMMON_UPDATE_INSTALLER_H
#define COMMON_UPDATE_INSTALLER_H

#include <QObject>

#include "common/update_info.h"

class UpdateInstaller final : public QObject
{
    Q_OBJECT

public:
    // Who the installer of the system is started for. The process already has the rights the
    // installation takes in both cases.
    enum class Mode
    {
        USER,   // A person asked for the update and is watching it.
        SERVICE // The service updates itself and nobody is watching.
    };

    // What came of starting the installation.
    enum class Result
    {
        STARTED, // The installer of the system is running. How it ended comes with sig_finished.
        DAMAGED, // The package is not the one the manifest describes.
        FAILED   // Anything else, the user dismissing the request for rights among it.
    };

    explicit UpdateInstaller(Mode mode, QObject* parent = nullptr);
    ~UpdateInstaller() final;

    // True when this system installs a package of |format| itself. The manifest names a format
    // the system may know nothing about, and then the update is left to the user.
    static bool isSupported(const QString& format);

    // True when the system lets this application install a package right now. Where it does not,
    // the user has to allow it first, and openInstallPermission takes them where that is done.
    static bool canInstall();
    static void openInstallPermission();

    // Removes the packages the updates before this one left behind. The installer of the system
    // reads the package after this process has let it go, so it cannot be taken away when the
    // installation starts.
    static void removeLeftovers();

    // Creates the file the package of |update_info| is to be downloaded into and returns its path.
    // An empty string is returned when it could not be created.
    QString createPackageFile(const UpdateInfo& update_info);

    // Checks the downloaded package against what the manifest says about it and starts the
    // installer of the system for it.
    Result install();

    // Removes the package and the directory it was prepared in, unless the installer of the system
    // has taken them. Calling it more than once is safe. The destructor calls it, so it is only
    // needed where the process ends before the deferred deletion of this object can run.
    void cleanup();

signals:
    // Emitted once when nothing more is going to happen. |error| is what the installer said when it
    // failed and is empty when there is nothing to report, as when the user declined.
    void sig_finished(bool success, const QString& error);

private:
    bool startInstaller();

    const Mode mode_;
    UpdateInfo update_info_;
    QString directory_;
    QString file_path_;

    Q_DISABLE_COPY_MOVE(UpdateInstaller)
};

#endif // COMMON_UPDATE_INSTALLER_H
