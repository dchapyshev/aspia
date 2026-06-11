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

#include "common/android/label.h"

#include <QEvent>

#include "common/android/controls.h"

namespace {

constexpr double kTitleFontScale = 1.6;
constexpr double kCaptionFontScale = 1.0;
constexpr double kCaptionOpacity = 0.6;

} // namespace

//--------------------------------------------------------------------------------------------------
Label::Label(QWidget* parent)
    : Label(QString(), Role::BODY, parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Label::Label(const QString& text, QWidget* parent)
    : Label(text, Role::BODY, parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Label::Label(const QString& text, Role role, QWidget* parent)
    : QLabel(text, parent),
      base_font_(font()),
      role_(role),
      applying_palette_(false)
{
    applyRole();
}

//--------------------------------------------------------------------------------------------------
Label::~Label() = default;

//--------------------------------------------------------------------------------------------------
void Label::setRole(Role role)
{
    if (role_ == role)
        return;

    role_ = role;
    applyRole();
}

//--------------------------------------------------------------------------------------------------
void Label::changeEvent(QEvent* event)
{
    QLabel::changeEvent(event);

    // The caption color is derived from the application palette, so it is recalculated when the
    // system theme changes.
    if (event->type() == QEvent::ApplicationPaletteChange && !applying_palette_)
        applyRole();
}

//--------------------------------------------------------------------------------------------------
void Label::applyRole()
{
    double scale = Controls::kFontScale;
    QFont::Weight weight = QFont::Normal;

    switch (role_)
    {
        case Role::TITLE:
            scale = kTitleFontScale;
            weight = QFont::DemiBold;
            break;

        case Role::BODY:
            break;

        case Role::CAPTION:
            scale = kCaptionFontScale;
            break;
    }

    QFont font = Controls::scaledFont(base_font_, scale);
    font.setWeight(weight);
    setFont(font);

    // The default-constructed palette follows the application palette, so assigning it resets
    // the color of the other roles back to the inherited one.
    QPalette palette;
    if (role_ == Role::CAPTION)
    {
        QColor muted = palette.color(QPalette::WindowText);
        muted.setAlphaF(kCaptionOpacity);
        palette.setColor(QPalette::WindowText, muted);
    }

    applying_palette_ = true;
    setPalette(palette);
    applying_palette_ = false;
}
