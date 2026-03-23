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

#ifndef CONSOLE_THEME_MANAGER_H
#define CONSOLE_THEME_MANAGER_H

#include <QApplication>
#include <QPalette>
#include <QProxyStyle>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>

namespace console {

class ThemeManager
{
public:
    struct Theme
    {
        QString id;
        QString style_name;
        QPalette palette;
    };

    static ThemeManager* instance();

    QStringList availableThemes() const;
    Theme getTheme(const QString& theme_id) const;
    void applyTheme(QApplication* app, const QString& theme_id);

private:
    class CustomStyleProxy final : public QProxyStyle
    {
    public:
        explicit CustomStyleProxy(QStyle* base_style, int small_icon_size)
            : QProxyStyle(base_style),
              small_icon_size_(small_icon_size)
        {}

        int pixelMetric(
            PixelMetric metric, const QStyleOption* option, const QWidget* widget) const final
        {
            if (metric == QStyle::PM_SmallIconSize)
                return small_icon_size_;

            return QProxyStyle::pixelMetric(metric, option, widget);
        }

    private:
        const int small_icon_size_;
    };

    ThemeManager() = default;

    QPalette createDarkPalette() const;
    QPalette createSystemPalette() const;
    QStyle* createLightStyle() const;

    int saved_icon_size_ = 20;
    bool icon_size_saved_ = false;

    Q_DISABLE_COPY_MOVE(ThemeManager)
};

} // namespace console

#endif // CONSOLE_THEME_MANAGER_H
