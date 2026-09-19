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

#ifndef BASE_UPDATE_CONSOLE_UPDATER_H
#define BASE_UPDATE_CONSOLE_UPDATER_H

#include <optional>

#include "base/update/update_info.h"

class QTextStream;

// The update of an application that has no interface of its own and is told what to do from the
// command line. Nothing here happens by itself: the one who administers the machine asks for it
// and reads what |out| says about it. Both calls return only when there is nothing left to wait
// for, so they belong in a process started to do this and nothing else.
class ConsoleUpdater
{
public:
    // |package| is the name the application goes by in the manifest of a release.
    ConsoleUpdater(const QString& package, QTextStream& out);
    ~ConsoleUpdater();

    // Reports what |channel| offers. Returns what the process is to exit with.
    int check(const QString& channel);

    // Downloads the update |channel| offers and hands it to the installer of the system. The
    // package stops the application, replaces the files and starts it again.
    int install(const QString& channel);

private:
    // Reads what |channel| offers for the package. No value at all means the check itself failed,
    // and an info that is not valid that there is nothing newer than what is installed.
    std::optional<UpdateInfo> checkForUpdates(const QString& channel);

    // Downloads the package of |update_info| into |file_path|. What stopped a download that did
    // not finish is put into |error|.
    bool download(const UpdateInfo& update_info, const QString& file_path, QString* error);

    // Prints the version that is installed and what |channel| offers over it. No value at all
    // means there is nothing to install, and |exit_code| is what the process is to exit with then.
    std::optional<UpdateInfo> offeredUpdate(const QString& channel, int* exit_code);

    const QString package_;
    QTextStream& out_;

    Q_DISABLE_COPY_MOVE(ConsoleUpdater)
};

#endif // BASE_UPDATE_CONSOLE_UPDATER_H
