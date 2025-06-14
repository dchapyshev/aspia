//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_WIN_DESKTOP_ENVIRONMENT_H
#define BASE_DESKTOP_WIN_DESKTOP_ENVIRONMENT_H

#include <QObject>

#include <optional>

namespace base {

class DesktopEnvironment : public QObject
{
    Q_OBJECT

public:
    explicit DesktopEnvironment(QObject* parent);
    virtual ~DesktopEnvironment();

    static DesktopEnvironment* create(QObject* parent = nullptr);

    void setWallpaper(bool enable);
    void setFontSmoothing(bool enable);
    void setEffects(bool enable);

    void onDesktopChanged();

protected:
    virtual void disableWallpaper() = 0;
    virtual void disableFontSmoothing() = 0;
    virtual void disableEffects() = 0;
    virtual void revertAll() = 0;

private:
    void applyNewSettings();

    std::optional<bool> wallpaper_;
    std::optional<bool> font_smoothing_;
    std::optional<bool> effects_;

    Q_DISABLE_COPY(DesktopEnvironment)
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DESKTOP_ENVIRONMENT_H
