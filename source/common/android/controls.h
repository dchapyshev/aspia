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

#ifndef COMMON_ANDROID_CONTROLS_H
#define COMMON_ANDROID_CONTROLS_H

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>

class QObject;
class QVariantAnimation;

class Controls
{
    Q_GADGET

public:
    // Text size multiplier shared by the touch controls. The platform fonts are too small for
    // touch targets, so they are scaled up by this factor.
    static constexpr double kFontScale = 1.2;

    // Returns a new state transition animation with the duration and easing shared by the touch
    // controls. The animation is owned by |parent|.
    static QVariantAnimation* createAnimation(QObject* parent);

    // Returns |font| with the size multiplied by |scale|. Handles fonts defined in points as well
    // as in pixels (the Android platform theme provides fonts with a pixel size only).
    static QFont scaledFont(const QFont& font, double scale);

    // Returns a linear interpolation between |from| and |to| by |progress| (0.0 to 1.0).
    static double lerp(double from, double to, double progress);

    // Returns a linear interpolation between |from| and |to| by |progress| (0.0 to 1.0).
    static QColor blend(const QColor& from, const QColor& to, double progress);

    // Returns a contrasting color (black or white) for text drawn on |background|, picked from
    // its luminance. The Android platform palette does not provide a usable highlighted text color.
    static QColor contrastColor(const QColor& background);

    // Color for error and destructive text. A fixed hex stays readable on both light and dark
    // surfaces; the platform palette has no error role.
    static QColor errorColor();

    // Renders the SVG at |svg_file_path| to |size| and recolors it with |color|, for monochrome
    // icons that follow the current palette.
    static QPixmap tintedPixmap(const QString& svg_file_path, const QSize& size,
                                const QColor& color);

private:
    Q_DISABLE_COPY_MOVE(Controls)
};

#endif // COMMON_ANDROID_CONTROLS_H
