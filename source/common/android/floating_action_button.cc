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

#include "common/android/floating_action_button.h"

#include <QMouseEvent>
#include <QPainter>

#include "common/android/controls.h"

namespace {

constexpr int kSize = 64;
constexpr int kMargin = 6;
constexpr int kIconSize = 24;
constexpr int kDragThreshold = 12;
constexpr double kIdleOpacity = 0.85;

} // namespace

//--------------------------------------------------------------------------------------------------
FloatingActionButton::FloatingActionButton(const QString& icon_file_path, QWidget* parent)
    : QWidget(parent),
      icon_file_path_(icon_file_path)
{
    setFixedSize(kSize, kSize);
    setCursor(Qt::PointingHandCursor);
}

//--------------------------------------------------------------------------------------------------
FloatingActionButton::~FloatingActionButton() = default;

//--------------------------------------------------------------------------------------------------
void FloatingActionButton::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(pressed_ ? 1.0 : kIdleOpacity);

    const QRectF circle = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);

    // A soft drop shadow built from stacked translucent ellipses.
    painter.setPen(Qt::NoPen);
    for (int i = kMargin; i > 0; --i)
    {
        QColor shadow(0, 0, 0);
        shadow.setAlphaF(0.03);
        painter.setBrush(shadow);
        painter.drawEllipse(circle.adjusted(-i, -i + 2, i, i + 2));
    }

    const QColor accent = Controls::accentColor();
    painter.setBrush(accent);
    painter.drawEllipse(circle);

    if (!icon_file_path_.isEmpty())
    {
        const QPixmap icon = Controls::tintedPixmap(icon_file_path_, QSize(kIconSize, kIconSize),
                                                    Controls::contrastColor(accent));
        painter.drawPixmap(QPointF(width() / 2.0 - kIconSize / 2.0, height() / 2.0 - kIconSize / 2.0),
                           icon);
    }
}

//--------------------------------------------------------------------------------------------------
void FloatingActionButton::mousePressEvent(QMouseEvent* event)
{
    pressed_ = true;
    dragging_ = false;
    press_offset_ = event->position().toPoint();
    press_global_ = event->globalPosition().toPoint();
    update();
}

//--------------------------------------------------------------------------------------------------
void FloatingActionButton::mouseMoveEvent(QMouseEvent* event)
{
    if (!pressed_)
        return;

    const QPoint global = event->globalPosition().toPoint();
    if (!dragging_ && (global - press_global_).manhattanLength() < kDragThreshold)
        return;

    dragging_ = true;

    QPoint pos = mapToParent(event->position().toPoint() - press_offset_);
    if (parentWidget())
    {
        const QRect bounds = parentWidget()->rect();
        pos.setX(qBound(bounds.left(), pos.x(), bounds.right() - width() + 1));
        pos.setY(qBound(bounds.top(), pos.y(), bounds.bottom() - height() + 1));
    }
    move(pos);
}

//--------------------------------------------------------------------------------------------------
void FloatingActionButton::mouseReleaseEvent(QMouseEvent* /* event */)
{
    const bool was_dragging = dragging_;
    pressed_ = false;
    dragging_ = false;
    update();

    if (!was_dragging)
        emit sig_clicked();
}
