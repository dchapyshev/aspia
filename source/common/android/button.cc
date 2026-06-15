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

#include "common/android/button.h"

#include <QPainter>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kButtonHeight = 40;
constexpr int kFilledHorizontalPadding = 20;
constexpr int kTextHorizontalPadding = 12;
constexpr int kMinWidth = 64;
constexpr double kDisabledOpacity = 0.38;
constexpr double kPressedLayerOpacity = 0.12;

} // namespace

//--------------------------------------------------------------------------------------------------
Button::Button(QWidget* parent)
    : Button(QString(), Role::FILLED, parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Button::Button(const QString& text, QWidget* parent)
    : Button(text, Role::FILLED, parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Button::Button(const QString& text, Role role, QWidget* parent)
    : QPushButton(text, parent),
      animation_(Controls::createAnimation(this)),
      press_progress_(0.0),
      role_(role)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        press_progress_ = value.toDouble();
        update();
    });
    connect(this, &QPushButton::pressed, this, &Button::onPressed);
    connect(this, &QPushButton::released, this, &Button::onReleased);
}

//--------------------------------------------------------------------------------------------------
Button::~Button() = default;

//--------------------------------------------------------------------------------------------------
void Button::setRole(Role role)
{
    if (role_ == role)
        return;

    role_ = role;
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------------------------------
void Button::setAccentColor(const QColor& color)
{
    accent_color_ = color;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize Button::sizeHint() const
{
    QFontMetrics fm(font());

    const int padding = (role_ == Role::FILLED) ? kFilledHorizontalPadding : kTextHorizontalPadding;
    int width = qMax(kMinWidth, fm.horizontalAdvance(text()) + padding * 2);
    return QSize(width, kButtonHeight);
}

//--------------------------------------------------------------------------------------------------
QSize Button::minimumSizeHint() const
{
    return sizeHint();
}

//--------------------------------------------------------------------------------------------------
void Button::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled())
        painter.setOpacity(kDisabledOpacity);

    const QRectF surface = rect();
    const double radius = surface.height() / 2.0;

    const QColor accent = accent_color_.isValid() ? accent_color_ : Controls::accentColor();
    const bool filled = (role_ == Role::FILLED);
    const QColor foreground = filled ? Controls::contrastColor(accent) : accent;

    painter.setPen(Qt::NoPen);

    if (filled)
    {
        painter.setBrush(accent);
        painter.drawRoundedRect(surface, radius, radius);
    }

    // The state layer is an overlay tinted with the foreground color that fades in while pressed.
    if (press_progress_ > 0.0)
    {
        QColor layer = foreground;
        layer.setAlphaF(kPressedLayerOpacity * press_progress_);

        painter.setBrush(layer);
        painter.drawRoundedRect(surface, radius, radius);
    }

    painter.setPen(foreground);
    painter.drawText(surface, Qt::AlignCenter, text());
}

//--------------------------------------------------------------------------------------------------
void Button::onPressed()
{
    animation_->stop();
    animation_->setStartValue(press_progress_);
    animation_->setEndValue(1.0);
    animation_->start();
}

//--------------------------------------------------------------------------------------------------
void Button::onReleased()
{
    animation_->stop();
    animation_->setStartValue(press_progress_);
    animation_->setEndValue(0.0);
    animation_->start();
}
