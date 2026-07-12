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

#ifndef HOST_DESKTOP_ENVIRONMENT_MAC_H
#define HOST_DESKTOP_ENVIRONMENT_MAC_H

#include <QHash>
#include <QString>

#include "host/desktop_environment.h"

class DesktopEnvironmentMac final : public DesktopEnvironment
{
public:
    explicit DesktopEnvironmentMac(QObject* parent = nullptr);
    ~DesktopEnvironmentMac() final;

protected:
    // DesktopEnvironment implementation.
    void disableWallpaper() final;
    void disableEffects() final;
    void revertAll() final;

private:
    // Original desktop image file per display id, saved before it is replaced with a solid color. An
    // empty value means the display had no picture (a solid background) to restore.
    QHash<quint32, QString> saved_wallpaper_;

    Q_DISABLE_COPY(DesktopEnvironmentMac)
};

#endif // HOST_DESKTOP_ENVIRONMENT_MAC_H
