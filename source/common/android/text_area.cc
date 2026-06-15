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

#include "common/android/text_area.h"

#include <QEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kCornerRadius = 6;
constexpr int kHorizontalPadding = 13;
constexpr int kLabelPadding = 4;
constexpr int kInnerVPadding = 8;
constexpr int kContentHeight = 92; // The text area portion, about three lines tall.
constexpr double kFloatedLabelScale = 0.78;
constexpr double kDisabledOpacity = 0.38;
constexpr double kMutedLabelOpacity = 0.55;

} // namespace

//--------------------------------------------------------------------------------------------------
TextArea::TextArea(QWidget* parent)
    : QWidget(parent),
      edit_(new QPlainTextEdit(this)),
      float_animation_(Controls::createAnimation(this)),
      focus_animation_(Controls::createAnimation(this))
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    edit_->setFont(font());
    edit_->setFrameShape(QFrame::NoFrame);
    edit_->setStyleSheet("QPlainTextEdit { background: transparent; }");
    edit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit_->installEventFilter(this);
    setFocusProxy(edit_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kHorizontalPadding, labelOverflow() + kInnerVPadding,
                               kHorizontalPadding, kInnerVPadding);
    layout->addWidget(edit_);

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
    connect(edit_, &QPlainTextEdit::textChanged, this, &TextArea::onTextChanged);
}

//--------------------------------------------------------------------------------------------------
TextArea::~TextArea() = default;

//--------------------------------------------------------------------------------------------------
void TextArea::setLabel(const QString& label)
{
    if (label_ == label)
        return;

    label_ = label;
    update();
}

//--------------------------------------------------------------------------------------------------
QString TextArea::text() const
{
    return edit_->toPlainText();
}

//--------------------------------------------------------------------------------------------------
void TextArea::setText(const QString& text)
{
    edit_->setPlainText(text);
}

//--------------------------------------------------------------------------------------------------
void TextArea::clear()
{
    edit_->clear();
}

//--------------------------------------------------------------------------------------------------
void TextArea::setPlaceholderText(const QString& text)
{
    edit_->setPlaceholderText(text);
}

//--------------------------------------------------------------------------------------------------
QSize TextArea::sizeHint() const
{
    return QSize(edit_->sizeHint().width() + 2 * kHorizontalPadding,
                 labelOverflow() + kContentHeight);
}

//--------------------------------------------------------------------------------------------------
QSize TextArea::minimumSizeHint() const
{
    return QSize(edit_->minimumSizeHint().width() + 2 * kHorizontalPadding,
                 labelOverflow() + kContentHeight);
}

//--------------------------------------------------------------------------------------------------
void TextArea::paintEvent(QPaintEvent* /* event */)
{
    // Focus transitions delivered before the widget is shown leave the animations at a stale
    // position, so the resting state is snapped to the actual one.
    if (float_animation_->state() != QAbstractAnimation::Running &&
        focus_animation_->state() != QAbstractAnimation::Running)
    {
        float_progress_ = (edit_->hasFocus() || !edit_->toPlainText().isEmpty()) ? 1.0 : 0.0;
        focus_progress_ = edit_->hasFocus() ? 1.0 : 0.0;
    }

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

        // At rest the label sits on the first text line (not centered in the tall field), and floats
        // onto the top outline.
        const double rest_y = field.top() + kInnerVPadding + QFontMetricsF(font()).height() / 2.0;

        label_rect = QRectF(0, 0, label_width + kLabelPadding * 2, label_metrics.height());
        label_rect.moveCenter(QPointF(
            label_center_x, Controls::lerp(rest_y, field.top(), float_progress_)));
    }

    // The outline thickens and is repainted with the accent color while the field has focus. The
    // floated label interrupts the outline stroke, so its area is clipped out.
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

//--------------------------------------------------------------------------------------------------
bool TextArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == edit_)
    {
        if (event->type() == QEvent::FocusIn)
        {
            animateFocus(true);
            updateFloatState();
        }
        else if (event->type() == QEvent::FocusOut)
        {
            animateFocus(false);
            updateFloatState();
        }
    }

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void TextArea::onTextChanged()
{
    updateFloatState();
}

//--------------------------------------------------------------------------------------------------
int TextArea::labelOverflow() const
{
    return QFontMetrics(Controls::scaledFont(font(), kFloatedLabelScale)).height() / 2;
}

//--------------------------------------------------------------------------------------------------
void TextArea::updateFloatState()
{
    const double target = (edit_->hasFocus() || !edit_->toPlainText().isEmpty()) ? 1.0 : 0.0;
    if (qFuzzyCompare(float_progress_, target))
        return;

    float_animation_->stop();
    float_animation_->setStartValue(float_progress_);
    float_animation_->setEndValue(target);
    float_animation_->start();
}

//--------------------------------------------------------------------------------------------------
void TextArea::animateFocus(bool focused)
{
    focus_animation_->stop();
    focus_animation_->setStartValue(focus_progress_);
    focus_animation_->setEndValue(focused ? 1.0 : 0.0);
    focus_animation_->start();
}
