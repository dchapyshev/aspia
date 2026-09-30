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

#ifndef HOST_WIN_MSI_PACKAGE_H
#define HOST_WIN_MSI_PACKAGE_H

#include <QString>

class MsiPackage
{
public:
    enum class Result
    {
        SUCCESS,
        NO_PACKAGE,
        UNSUPPORTED_PACKAGE,
        FAILED
    };

    // Saves the installed host package to |file_path| with the current host settings built in.
    static Result exportWithSettings(const QString& file_path);

private:
    Q_DISABLE_COPY_MOVE(MsiPackage)
};

#endif // HOST_WIN_MSI_PACKAGE_H
