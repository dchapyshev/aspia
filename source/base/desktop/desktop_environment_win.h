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

#ifndef BASE_DESKTOP_DESKTOP_ENVIRONMENT_WIN_H
#define BASE_DESKTOP_DESKTOP_ENVIRONMENT_WIN_H

#include "base/desktop/desktop_environment.h"

namespace base {

class DesktopEnvironmentWin final : public DesktopEnvironment
{
    Q_OBJECT

public:
    explicit DesktopEnvironmentWin(QObject* parent = nullptr);
    ~DesktopEnvironmentWin() final;

    static void updateEnvironment();

protected:
    void disableWallpaper() final;
    void disableFontSmoothing() final;
    void disableEffects() final;
    void revertAll() final;

private:
    bool drop_shadow_changed_ = false;
    bool animation_changed_ = false;

    Q_DISABLE_COPY(DesktopEnvironmentWin)
};

} // namespace base

#endif // BASE_DESKTOP_DESKTOP_ENVIRONMENT_WIN_H
