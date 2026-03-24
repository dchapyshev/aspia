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
#include <QStyleHints>

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
    return { "auto", "light", "dark" };
}

//--------------------------------------------------------------------------------------------------
void ThemeManager::applyTheme(QApplication* app, const QString& theme_id)
{
    if (!app)
        return;

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

    ensureStyleCreated(app);

    if (theme_id == "auto")
    {
        // Let the system decide the color scheme.
        app->styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
        app->setPalette(QPalette());
        return;
    }

    bool is_dark = (theme_id == "dark");

    app->styleHints()->setColorScheme(is_dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);

    // Native styles (windows11, macos) handle dark/light mode via colorScheme.
    // Fusion requires a custom palette for dark mode.
    if (!is_native_style_ && is_dark)
        app->setPalette(createDarkPalette());
    else
        app->setPalette(QPalette());
}

//--------------------------------------------------------------------------------------------------
QString ThemeManager::themeName(const QString& theme_id)
{
    if (theme_id == "dark")
        return tr("Dark");
    else if (theme_id == "light")
        return tr("Light");
    return tr("Auto");
}

//--------------------------------------------------------------------------------------------------
QPalette ThemeManager::createDarkPalette() const
{
    QPalette palette;

    // Window and base colors.
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(35, 35, 35));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);

    // Text colors.
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));

    // Tooltip colors.
    palette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipText, Qt::white);

    // Link colors.
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::LinkVisited, QColor(128, 90, 220));

    // Selection and accent colors.
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    palette.setColor(QPalette::Accent, QColor(42, 130, 218));

    // 3D border colors (used by Fusion and classic styles).
    palette.setColor(QPalette::Light, QColor(80, 80, 80));
    palette.setColor(QPalette::Midlight, QColor(67, 67, 67));
    palette.setColor(QPalette::Mid, QColor(42, 42, 42));
    palette.setColor(QPalette::Dark, QColor(30, 30, 30));
    palette.setColor(QPalette::Shadow, QColor(20, 20, 20));

    // Disabled state.
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(42, 42, 42));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(42, 42, 42));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(80, 80, 80));

    return palette;
}

//--------------------------------------------------------------------------------------------------
void ThemeManager::ensureStyleCreated(QApplication* app)
{
    if (style_created_)
        return;

    style_created_ = true;

    QStyle* base_style = nullptr;

#if defined(Q_OS_WINDOWS)
    base_style = QStyleFactory::create("windows11");
#elif defined(Q_OS_MACOS)
    base_style = QStyleFactory::create("macos");
#endif

    if (base_style)
    {
        is_native_style_ = true;
    }
    else
    {
        base_style = QStyleFactory::create("Fusion");
        if (!base_style)
            base_style = QStyleFactory::create(QString());

        is_native_style_ = false;
    }

    app->setStyle(new CustomStyleProxy(base_style, saved_icon_size_));
}

} // namespace console
