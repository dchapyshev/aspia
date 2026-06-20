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

#include "common/android/segmented_button.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <utility>

#include "common/android/controls.h"

namespace {

constexpr int kHeight = 48;
constexpr int kRadius = 12;
constexpr int kHorizontalPadding = 16;
constexpr int kMinSegmentWidth = 80;
constexpr double kOutlineWidth = 1.0;
constexpr double kActiveLayerOpacity = 0.14;
constexpr double kPressLayerOpacity = 0.07;

} // namespace

//--------------------------------------------------------------------------------------------------
SegmentedButton::SegmentedButton(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(Controls::scaledFont(font(), Controls::kFontScale));
}

//--------------------------------------------------------------------------------------------------
SegmentedButton::~SegmentedButton() = default;

//--------------------------------------------------------------------------------------------------
void SegmentedButton::addSegment(const QString& text)
{
    segments_.append(text);
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------------------------------
void SegmentedButton::setCurrentIndex(int index)
{
    if (index < 0 || index >= segments_.size() || index == current_)
        return;

    current_ = index;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize SegmentedButton::sizeHint() const
{
    QFontMetrics metrics(font());

    int width = 0;
    for (const QString& text : std::as_const(segments_))
        width += qMax(kMinSegmentWidth, metrics.horizontalAdvance(text) + 2 * kHorizontalPadding);

    return QSize(width, kHeight);
}

//--------------------------------------------------------------------------------------------------
void SegmentedButton::paintEvent(QPaintEvent* /* event */)
{
    if (segments_.isEmpty())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = QRectF(rect()).adjusted(kOutlineWidth / 2.0, kOutlineWidth / 2.0,
                                                  -kOutlineWidth / 2.0, -kOutlineWidth / 2.0);
    const QColor outline = palette().color(QPalette::Mid);
    const QColor accent = Controls::accentColor();
    const int count = segments_.size();
    const double segment_width = bounds.width() / count;

    // State-layer fills, clipped to the rounded outline so the active segment nests inside it.
    QPainterPath shape;
    shape.addRoundedRect(bounds, kRadius, kRadius);
    painter.setClipPath(shape);

    for (int i = 0; i < count; ++i)
    {
        double opacity = 0.0;
        if (i == current_)
            opacity = kActiveLayerOpacity;
        else if (i == pressed_)
            opacity = kPressLayerOpacity;

        if (opacity > 0.0)
        {
            QColor fill = accent;
            fill.setAlphaF(opacity);
            painter.fillRect(
                QRectF(bounds.left() + i * segment_width, bounds.top(), segment_width, bounds.height()),
                fill);
        }
    }

    painter.setClipping(false);

    // The shared outline and the dividers between segments.
    painter.setPen(QPen(outline, kOutlineWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bounds, kRadius, kRadius);

    for (int i = 1; i < count; ++i)
    {
        const double x = bounds.left() + i * segment_width;
        painter.drawLine(QPointF(x, bounds.top()), QPointF(x, bounds.bottom()));
    }

    // Labels: the active one in the accent color, the rest in the regular text color.
    for (int i = 0; i < count; ++i)
    {
        const QRectF segment(bounds.left() + i * segment_width, bounds.top(),
                             segment_width, bounds.height());
        const bool active = (i == current_);

        QFont label_font = font();
        label_font.setWeight(active ? QFont::Medium : QFont::Normal);
        painter.setFont(label_font);
        painter.setPen(active ? accent : palette().color(QPalette::WindowText));

        const QString text = painter.fontMetrics().elidedText(
            segments_[i], Qt::ElideRight, int(segment_width - 2 * kHorizontalPadding));
        painter.drawText(segment, Qt::AlignCenter, text);
    }
}

//--------------------------------------------------------------------------------------------------
void SegmentedButton::mousePressEvent(QMouseEvent* event)
{
    pressed_ = segmentAt(event->position().toPoint());
    update();
}

//--------------------------------------------------------------------------------------------------
void SegmentedButton::mouseReleaseEvent(QMouseEvent* event)
{
    const int segment = segmentAt(event->position().toPoint());
    pressed_ = -1;

    if (segment >= 0 && segment != current_)
    {
        current_ = segment;
        emit sig_currentChanged(current_);
    }

    update();
}

//--------------------------------------------------------------------------------------------------
int SegmentedButton::segmentAt(const QPoint& pos) const
{
    if (segments_.isEmpty() || !rect().contains(pos))
        return -1;

    const int index = pos.x() * segments_.size() / width();
    return qBound(0, index, static_cast<int>(segments_.size()) - 1);
}
