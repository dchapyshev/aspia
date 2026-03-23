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

#include "console/theme_manager.h"

#include <QStyleFactory>

namespace console {

//--------------------------------------------------------------------------------------------------
ThemeManager* ThemeManager::instance()
{
    static ThemeManager theme_manager;
    return &theme_manager;
}

//--------------------------------------------------------------------------------------------------
QStringList ThemeManager::availableThemes() const
{
    return { QStringLiteral("light"), QStringLiteral("dark") };
}

//--------------------------------------------------------------------------------------------------
ThemeManager::Theme ThemeManager::getTheme(const QString& theme_id) const
{
    if (theme_id == QStringLiteral("dark"))
    {
        return {
            QStringLiteral("dark"),
            QStringLiteral("Fusion"),
            createDarkPalette()
        };
    }

    return {
        QStringLiteral("light"),
        QString(),
        createSystemPalette()
    };
}

//--------------------------------------------------------------------------------------------------
void ThemeManager::applyTheme(QApplication* app, const QString& theme_id)
{
    if (!app)
        return;

    const Theme theme = getTheme(theme_id);

    if (!theme.style_name.isEmpty())
    {
        if (!icon_size_saved_ && app->style())
        {
            QStyleOption option;
            saved_icon_size_ = app->style()->pixelMetric(QStyle::PM_SmallIconSize, &option, nullptr);

            if (saved_icon_size_ <= 0)
            {
                if (qEnvironmentVariableIsSet("ASPIA_SMALL_ICON_SIZE"))
                    saved_icon_size_ = qEnvironmentVariableIntValue("ASPIA_SMALL_ICON_SIZE");

                if (saved_icon_size_ < 16)
                    saved_icon_size_ = 16;
                else if (saved_icon_size_ > 48)
                    saved_icon_size_ = 48;

                if (saved_icon_size_ <= 0)
                    saved_icon_size_ = 20;
            }

            icon_size_saved_ = true;
        }

        if (QStyle* style = QStyleFactory::create(theme.style_name))
            app->setStyle(style);

        app->setPalette(theme.palette);
        return;
    }

    if (QStyle* style = createLightStyle())
        app->setStyle(style);

    app->setPalette(createSystemPalette());
}

QPalette ThemeManager::createDarkPalette() const
{
    QPalette palette;

    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(35, 35, 35));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));

    return palette;
}

//--------------------------------------------------------------------------------------------------
QPalette ThemeManager::createSystemPalette() const
{
    return QPalette();
}

//--------------------------------------------------------------------------------------------------
QStyle* ThemeManager::createLightStyle() const
{
    QStyle* base_style = nullptr;

#if defined(Q_OS_WIN)
    base_style = QStyleFactory::create(QStringLiteral("Windows"));
#elif defined(Q_OS_MAC)
    base_style = QStyleFactory::create(QStringLiteral("macos"));
#endif

    if (!base_style)
        base_style = QStyleFactory::create(QStringLiteral("Fusion"));

    if (!base_style)
        base_style = QStyleFactory::create(QString());

    return new CustomStyleProxy(base_style, saved_icon_size_);
}

} // namespace console
