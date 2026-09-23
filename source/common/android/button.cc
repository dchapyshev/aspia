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
#include <QPainterPath>

#include "common/android/animation.h"
#include "common/android/controls.h"

namespace {

constexpr int kButtonHeight = 40;
constexpr int kFilledHorizontalPadding = 20;
constexpr int kTextHorizontalPadding = 12;
constexpr int kMinWidth = 64;
constexpr int kOutlineWidth = 1;
constexpr double kDisabledOpacity = 0.38;
constexpr double kPressedLayerOpacity = 0.12;
constexpr double kProgressTrackOpacity = 0.2;

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
      role_(role),
      progress_(-1)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    // Taking the focus on a press would move it away from the field being edited, which closes the
    // software keyboard: the layout then reflows under the finger and the release misses the
    // button, so the first tap would be swallowed.
    setFocusPolicy(Qt::NoFocus);

    connect(animation_, &Animation::sig_valueChanged, this, [this](double value)
    {
        press_progress_ = value;
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
    updateMouseEvents();
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
void Button::setProgress(int percentage)
{
    percentage = qMin(percentage, 100);
    if (progress_ == percentage)
        return;

    progress_ = percentage;
    updateMouseEvents();
    update();
}

//--------------------------------------------------------------------------------------------------
QSize Button::sizeHint() const
{
    QFontMetrics fm(font());

    const int padding = (role_ == Role::TEXT) ? kTextHorizontalPadding : kFilledHorizontalPadding;
    int width = qMax(kMinWidth, fm.horizontalAdvance(text()) + padding * 2);
    return QSize(width, kButtonHeight);
}

//--------------------------------------------------------------------------------------------------
QSize Button::minimumSizeHint() const
{
    // Allow the layout to shrink the button below its text width (a long caption then elides in
    // paintEvent) so it cannot overflow a narrow container.
    return QSize(kMinWidth, kButtonHeight);
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

    const bool progress = isProgressShown();

    painter.setPen(Qt::NoPen);

    QRectF done;
    if (progress)
    {
        done = surface;
        done.setWidth(surface.width() * progress_ / 100.0);
        if (layoutDirection() == Qt::RightToLeft)
            done.moveRight(surface.right());

        QColor track = accent;
        track.setAlphaF(kProgressTrackOpacity);

        QPainterPath shape;
        shape.addRoundedRect(surface, radius, radius);

        painter.setClipPath(shape);
        painter.fillRect(surface, track);
        painter.fillRect(done, accent);
    }
    else if (filled)
    {
        painter.setBrush(accent);
        painter.drawRoundedRect(surface, radius, radius);
    }
    else if (role_ == Role::OUTLINED)
    {
        const QRectF outline = surface.adjusted(kOutlineWidth / 2.0, kOutlineWidth / 2.0,
                                                -kOutlineWidth / 2.0, -kOutlineWidth / 2.0);

        painter.setPen(QPen(palette().color(QPalette::Mid), kOutlineWidth));
        painter.drawRoundedRect(outline, radius, radius);
        painter.setPen(Qt::NoPen);
    }

    // The state layer is an overlay tinted with the foreground color that fades in while pressed.
    if (press_progress_ > 0.0)
    {
        QColor layer = foreground;
        layer.setAlphaF(kPressedLayerOpacity * press_progress_);

        painter.setBrush(layer);
        painter.drawRoundedRect(surface, radius, radius);
    }

    // sizeHint already reserves the horizontal padding, and drawText centers within the full surface,
    // so elide against the whole width - only a genuinely shrunk button (long caption, tight layout)
    // then gets the ellipsis.
    const QString shown = painter.fontMetrics().elidedText(
        progress ? QString("%1%").arg(progress_) : text(), Qt::ElideRight, int(surface.width()));

    // Over the track the text takes the accent, and only the part over the done fill stays in the
    // contrast color.
    if (progress)
    {
        painter.setPen(accent);
        painter.drawText(surface, Qt::AlignCenter, shown);
        painter.setClipRect(done, Qt::IntersectClip);
    }

    painter.setPen(foreground);
    painter.drawText(surface, Qt::AlignCenter, shown);
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

//--------------------------------------------------------------------------------------------------
bool Button::isProgressShown() const
{
    return role_ == Role::FILLED && progress_ >= 0;
}

//--------------------------------------------------------------------------------------------------
void Button::updateMouseEvents()
{
    setAttribute(Qt::WA_TransparentForMouseEvents, isProgressShown());
}
