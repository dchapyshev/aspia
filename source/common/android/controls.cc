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

#include "common/android/controls.h"

#include <QApplication>
#include <QPainter>
#include <QVariantAnimation>

#include <cmath>

#include "base/gui_application.h"

namespace {

constexpr int kAnimationDurationMs = 150;

// Minimum contrast ratio between the accent and the background, matching the WCAG AA threshold for
// normal text so accent labels stay legible.
constexpr double kMinAccentContrast = 4.5;

//--------------------------------------------------------------------------------------------------
double relativeLuminance(const QColor& color)
{
    auto channel = [](double value)
    {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) +
           0.0722 * channel(color.blueF());
}

//--------------------------------------------------------------------------------------------------
double contrastRatio(const QColor& first, const QColor& second)
{
    const double a = relativeLuminance(first);
    const double b = relativeLuminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QVariantAnimation* Controls::createAnimation(QObject* parent)
{
    QVariantAnimation* animation = new QVariantAnimation(parent);
    animation->setDuration(kAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    return animation;
}

//--------------------------------------------------------------------------------------------------
// static
QFont Controls::scaledFont(const QFont& font, double scale)
{
    QFont result = font;

    if (font.pointSizeF() > 0)
        result.setPointSizeF(font.pointSizeF() * scale);
    else
        result.setPixelSize(qMax(1, qRound(font.pixelSize() * scale)));

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
double Controls::lerp(double from, double to, double progress)
{
    return from + (to - from) * progress;
}

//--------------------------------------------------------------------------------------------------
// static
QColor Controls::blend(const QColor& from, const QColor& to, double progress)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * progress,
                            from.greenF() + (to.greenF() - from.greenF()) * progress,
                            from.blueF() + (to.blueF() - from.blueF()) * progress);
}

//--------------------------------------------------------------------------------------------------
// static
QColor Controls::contrastColor(const QColor& background)
{
    const double luminance =
        0.299 * background.redF() + 0.587 * background.greenF() + 0.114 * background.blueF();
    return luminance > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
}

//--------------------------------------------------------------------------------------------------
// static
QColor Controls::errorColor()
{
    return QColor(0xE5, 0x48, 0x4D);
}

//--------------------------------------------------------------------------------------------------
// static
QColor Controls::accentColor()
{
    // The application palette is read directly so the accent follows the current theme even on the
    // first paint after a theme change, when a cached widget palette may still be stale.
    const QPalette palette = QApplication::palette();
    const QColor accent = palette.color(QPalette::Highlight);
    const QColor background = palette.color(QPalette::Window);

    if (contrastRatio(accent, background) >= kMinAccentContrast)
        return accent;

    // Move the lightness away from the background (darker on light themes, lighter on dark ones)
    // until the accent reads clearly, keeping the system hue and saturation.
    const int step = relativeLuminance(background) > 0.5 ? -4 : 4;

    int h, s, l, a;
    accent.getHsl(&h, &s, &l, &a);

    QColor adjusted = accent;
    while (contrastRatio(adjusted, background) < kMinAccentContrast)
    {
        const int next = l + step;
        if (next < 0 || next > 255)
            break;

        l = next;
        adjusted.setHsl(h, s, l, a);
    }

    return adjusted;
}

//--------------------------------------------------------------------------------------------------
// static
QColor Controls::surfaceColor()
{
    const QPalette palette = QApplication::palette();
    return blend(palette.color(QPalette::Window), palette.color(QPalette::WindowText), 0.08);
}

//--------------------------------------------------------------------------------------------------
// static
QPixmap Controls::tintedPixmap(const QString& svg_file_path, const QSize& size, const QColor& color)
{
    QPixmap pixmap = GuiApplication::svgPixmap(svg_file_path, size);
    if (pixmap.isNull())
        return pixmap;

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}
