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

#include "common/android/line_edit.h"

#include <QPainter>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kFieldHeight = 46;
constexpr int kCornerRadius = 6;
constexpr int kHorizontalPadding = 13;
constexpr int kLabelPadding = 4;
constexpr double kFloatedLabelScale = 0.78;
constexpr double kDisabledOpacity = 0.38;
constexpr double kMutedLabelOpacity = 0.55;

} // namespace

//--------------------------------------------------------------------------------------------------
LineEdit::LineEdit(QWidget* parent)
    : QLineEdit(parent),
      float_animation_(Controls::createAnimation(this)),
      focus_animation_(Controls::createAnimation(this)),
      float_progress_(0.0),
      focus_progress_(0.0)
{
    setFrame(false);
    setStyleSheet("LineEdit { background: transparent; border: none; }");

    // Android draws draggable text handles (the teardrop under the cursor) in a separate popup
    // window that frequently lingers on screen after the field or its dialog is gone. Suppress the
    // handles - tapping still positions the cursor.
    setInputMethodHints(inputMethodHints() | Qt::ImhNoTextHandles);

    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    // The top text margin keeps the text vertically centered in the field, which starts below
    // the floating label overflow area.
    setTextMargins(kHorizontalPadding, labelOverflow(), kHorizontalPadding, 0);

    connect(float_animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        float_progress_ = value.toDouble();
        update();
    });
    connect(focus_animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        focus_progress_ = value.toDouble();
        update();
    });
    connect(this, &QLineEdit::textChanged, this, &LineEdit::onTextChanged);
}

//--------------------------------------------------------------------------------------------------
LineEdit::~LineEdit() = default;

//--------------------------------------------------------------------------------------------------
void LineEdit::setLabel(const QString& label)
{
    if (label_ == label)
        return;

    label_ = label;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize LineEdit::sizeHint() const
{
    return QSize(QLineEdit::sizeHint().width(), kFieldHeight + labelOverflow());
}

//--------------------------------------------------------------------------------------------------
QSize LineEdit::minimumSizeHint() const
{
    return QSize(QLineEdit::minimumSizeHint().width(), kFieldHeight + labelOverflow());
}

//--------------------------------------------------------------------------------------------------
void LineEdit::paintEvent(QPaintEvent* event)
{
    // Focus transitions delivered before the widget is shown leave the animations at a stale
    // position, so the resting state is snapped to the actual one.
    if (float_animation_->state() != QAbstractAnimation::Running &&
        focus_animation_->state() != QAbstractAnimation::Running)
    {
        float_progress_ = (hasFocus() || !text().isEmpty()) ? 1.0 : 0.0;
        focus_progress_ = hasFocus() ? 1.0 : 0.0;
    }

    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (!isEnabled())
            painter.setOpacity(kDisabledOpacity);

        const QColor accent = Controls::accentColor();
        const QColor outline = palette().color(QPalette::Mid);

        QRectF field = rect();
        field.setTop(labelOverflow());

        QFont label_font = font();
        QRectF label_rect;
        if (!label_.isEmpty())
        {
            label_font = Controls::scaledFont(
                font(), Controls::lerp(1.0, kFloatedLabelScale, float_progress_));

            const QFontMetricsF label_metrics(label_font);
            const double label_width = label_metrics.horizontalAdvance(label_);
            const double label_center_x = (layoutDirection() == Qt::RightToLeft) ?
                field.right() - kHorizontalPadding - kLabelPadding - label_width / 2.0 :
                field.left() + kHorizontalPadding + kLabelPadding + label_width / 2.0;

            label_rect = QRectF(0, 0, label_width + kLabelPadding * 2, label_metrics.height());
            label_rect.moveCenter(QPointF(
                label_center_x, Controls::lerp(field.center().y(), field.top(), float_progress_)));
        }

        // The outline thickens and is repainted with the accent color while the field has focus.
        // The floated label interrupts the outline stroke, so its area is clipped out.
        painter.save();
        if (!label_rect.isNull() && float_progress_ > 0.0)
            painter.setClipRegion(QRegion(rect()) - QRegion(label_rect.toAlignedRect()));

        const double border_width = Controls::lerp(1.0, 2.0, focus_progress_);
        painter.setPen(QPen(Controls::blend(outline, accent, focus_progress_), border_width));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(field.adjusted(border_width / 2.0, border_width / 2.0,
                                               -border_width / 2.0, -border_width / 2.0),
                                kCornerRadius, kCornerRadius);
        painter.restore();

        if (!label_rect.isNull())
        {
            // At rest the label is muted like a placeholder and turns opaque accent on focus.
            const double active = qMin(float_progress_, focus_progress_);
            QColor label_color = Controls::blend(palette().color(QPalette::WindowText), accent, active);
            label_color.setAlphaF(Controls::lerp(kMutedLabelOpacity, 1.0, active));

            painter.setFont(label_font);
            painter.setPen(label_color);
            painter.drawText(label_rect, Qt::AlignCenter, label_);
        }
    }

    QLineEdit::paintEvent(event);
}

//--------------------------------------------------------------------------------------------------
void LineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);

    focus_animation_->stop();
    focus_animation_->setStartValue(focus_progress_);
    focus_animation_->setEndValue(1.0);
    focus_animation_->start();

    updateFloatState();
}

//--------------------------------------------------------------------------------------------------
void LineEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);

    focus_animation_->stop();
    focus_animation_->setStartValue(focus_progress_);
    focus_animation_->setEndValue(0.0);
    focus_animation_->start();

    updateFloatState();
}

//--------------------------------------------------------------------------------------------------
void LineEdit::onTextChanged(const QString& /* text */)
{
    updateFloatState();
}

//--------------------------------------------------------------------------------------------------
int LineEdit::labelOverflow() const
{
    return QFontMetrics(Controls::scaledFont(font(), kFloatedLabelScale)).height() / 2;
}

//--------------------------------------------------------------------------------------------------
void LineEdit::updateFloatState()
{
    const double target = (hasFocus() || !text().isEmpty()) ? 1.0 : 0.0;
    if (qFuzzyCompare(float_progress_, target))
        return;

    float_animation_->stop();
    float_animation_->setStartValue(float_progress_);
    float_animation_->setEndValue(target);
    float_animation_->start();
}
