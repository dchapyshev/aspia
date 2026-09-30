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

#ifndef HOST_WIN_PORTABLE_PACKAGE_H
#define HOST_WIN_PORTABLE_PACKAGE_H

#include <QByteArray>
#include <QString>

#include <optional>

#include "base/build_config.h"
#include "base/net/address.h"

class PortablePackage
{
public:
    enum class Result
    {
        SUCCESS,
        NO_ROUTER,
        FAILED
    };

    struct Settings
    {
        Address router_address { kDefaultRouterHostTcpPort };
        QByteArray router_public_key;
    };

    // Saves a copy of the host executable to |file_path| with the router settings of the host
    // built in.
    static Result exportWithSettings(const QString& file_path);

    // Returns the settings built into the executable of the current process, if there are any.
    static std::optional<Settings> builtInSettings();

private:
    Q_DISABLE_COPY_MOVE(PortablePackage)
};

#endif // HOST_WIN_PORTABLE_PACKAGE_H
