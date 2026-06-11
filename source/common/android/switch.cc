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

#include "common/android/switch.h"

#include <QPainter>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kTrackWidth = 42;
constexpr int kTrackHeight = 26;
constexpr int kThumbUncheckedSize = 12;
constexpr int kThumbCheckedSize = 20;
constexpr int kStateLayerSize = 32;
constexpr int kTextSpacing = 10;
constexpr int kMinTargetSize = 40;
constexpr double kDisabledOpacity = 0.38;

} // namespace

//--------------------------------------------------------------------------------------------------
Switch::Switch(QWidget* parent)
    : Switch(QString(), parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Switch::Switch(const QString& text, QWidget* parent)
    : QCheckBox(text, parent),
      animation_(Controls::createAnimation(this)),
      progress_(isChecked() ? 1.0 : 0.0)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    // The label wraps to several lines when it does not fit, so the height follows the width.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        progress_ = value.toDouble();
        update();
    });
    connect(this, &QCheckBox::toggled, this, &Switch::onToggled);
}

//--------------------------------------------------------------------------------------------------
Switch::~Switch() = default;

//--------------------------------------------------------------------------------------------------
QSize Switch::sizeHint() const
{
    QFontMetrics fm(font());

    int width = kTrackWidth;
    if (!text().isEmpty())
        width += kTextSpacing + fm.horizontalAdvance(text());

    return QSize(width, qMax(kMinTargetSize, fm.height()));
}

//--------------------------------------------------------------------------------------------------
QSize Switch::minimumSizeHint() const
{
    // A small width so the layout can shrink the control and wrap the label instead of forcing it
    // wider than the screen.
    return QSize(kTrackWidth + kTextSpacing, qMax(kMinTargetSize, QFontMetrics(font()).height()));
}

//--------------------------------------------------------------------------------------------------
int Switch::heightForWidth(int width) const
{
    QFontMetrics fm(font());

    if (text().isEmpty())
        return qMax(kMinTargetSize, fm.height());

    const int text_width = width - kTrackWidth - kTextSpacing;
    if (text_width <= 0)
        return qMax(kMinTargetSize, fm.height());

    const QRect bounds = fm.boundingRect(QRect(0, 0, text_width, 0),
                                         Qt::TextWordWrap | Qt::AlignLeft, text());
    return qMax(kMinTargetSize, bounds.height());
}

//--------------------------------------------------------------------------------------------------
void Switch::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled())
        painter.setOpacity(kDisabledOpacity);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);

    QRectF track(0, 0, kTrackWidth, kTrackHeight);
    track.moveTop(rect().center().y() - kTrackHeight / 2.0);
    if (rtl)
        track.moveRight(rect().right());
    else
        track.moveLeft(rect().left());

    const QColor accent = palette().color(QPalette::Highlight);
    const QColor outline = palette().color(QPalette::Mid);
    const QColor window = palette().color(QPalette::Window);
    const QColor window_text = palette().color(QPalette::WindowText);
    const QColor thumb_off = Controls::blend(window, window_text, 0.6);
    const QColor thumb_on = Controls::contrastColor(accent);

    // The thumb slides between the track edges, growing as it turns on.
    double thumb_progress = rtl ? (1.0 - progress_) : progress_;
    double thumb_size = Controls::lerp(kThumbUncheckedSize, kThumbCheckedSize, progress_);
    QPointF thumb_center(Controls::lerp(track.left() + kTrackHeight / 2.0,
                                        track.right() - kTrackHeight / 2.0, thumb_progress),
                         track.center().y());

    // State layer around the thumb while the button is pressed.
    if (isDown())
    {
        QColor layer = accent;
        layer.setAlphaF(0.12);

        painter.setPen(Qt::NoPen);
        painter.setBrush(layer);
        painter.drawEllipse(thumb_center, kStateLayerSize / 2.0, kStateLayerSize / 2.0);
    }

    // The outlined track is filled with the accent color as the animation progresses.
    QColor fill = accent;
    fill.setAlphaF(progress_);

    painter.setPen(QPen(Controls::blend(outline, accent, progress_), 2));
    painter.setBrush(fill);
    painter.drawRoundedRect(track.adjusted(1, 1, -1, -1), kTrackHeight / 2.0 - 1,
                            kTrackHeight / 2.0 - 1);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Controls::blend(thumb_off, thumb_on, progress_));
    painter.drawEllipse(thumb_center, thumb_size / 2.0, thumb_size / 2.0);

    if (!text().isEmpty())
    {
        QRectF text_rect = rect();
        if (rtl)
            text_rect.setRight(track.left() - kTextSpacing);
        else
            text_rect.setLeft(track.right() + kTextSpacing);

        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(text_rect, Qt::TextWordWrap | Qt::AlignVCenter |
                                        (rtl ? Qt::AlignRight : Qt::AlignLeft), text());
    }
}

//--------------------------------------------------------------------------------------------------
bool Switch::hitButton(const QPoint& pos) const
{
    return rect().contains(pos);
}

//--------------------------------------------------------------------------------------------------
void Switch::onToggled(bool checked)
{
    animation_->stop();
    animation_->setStartValue(progress_);
    animation_->setEndValue(checked ? 1.0 : 0.0);
    animation_->start();
}
