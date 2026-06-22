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

#include "base/x11/x11_util.h"

#include <QFile>
#include <QFileInfo>

#include <pwd.h>

#include <filesystem>

//--------------------------------------------------------------------------------------------------
// static
QString X11Util::xauthorityForUser(const QString& user_name)
{
    const struct passwd* pw = getpwnam(user_name.toLocal8Bit().constData());
    if (!pw)
        return QString();

    const QString run_user = QString("/run/user/%1/").arg(pw->pw_uid);

    // Read the -auth argument of the user's running X server (independent of the display manager).
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/proc", error))
    {
        QFile cmdline(QString::fromStdString(entry.path().string()) + "/cmdline");
        if (!cmdline.open(QIODevice::ReadOnly))
            continue;

        const QList<QByteArray> args = cmdline.readAll().split('\0');
        if (args.isEmpty())
            continue;

        const QString program = QFileInfo(QString::fromLocal8Bit(args.constFirst())).fileName();
        if (program != "Xorg" && program != "X")
            continue;

        const qsizetype index = args.indexOf("-auth");
        if (index < 0 || index + 1 >= args.size())
            continue;

        const QString auth = QString::fromLocal8Bit(args.at(index + 1));
        if (auth.startsWith(run_user))
            return auth;
    }

    const QString gdm_auth = run_user + "gdm/Xauthority";
    if (QFileInfo::exists(gdm_auth))
        return gdm_auth;

    return QString::fromLocal8Bit(pw->pw_dir) + "/.Xauthority";
}
