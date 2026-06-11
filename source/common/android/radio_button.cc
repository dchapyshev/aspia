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

#include "common/android/radio_button.h"

#include <QPainter>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kRingSize = 20;
constexpr int kRingWidth = 2;
constexpr int kDotSize = 10;
constexpr int kStateLayerSize = 32;
constexpr int kTextSpacing = 10;
constexpr int kMinTargetSize = 40;
constexpr double kDisabledOpacity = 0.38;

} // namespace

//--------------------------------------------------------------------------------------------------
RadioButton::RadioButton(QWidget* parent)
    : RadioButton(QString(), parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
RadioButton::RadioButton(const QString& text, QWidget* parent)
    : QRadioButton(text, parent),
      animation_(Controls::createAnimation(this)),
      progress_(isChecked() ? 1.0 : 0.0)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        progress_ = value.toDouble();
        update();
    });
    connect(this, &QRadioButton::toggled, this, &RadioButton::onToggled);
}

//--------------------------------------------------------------------------------------------------
RadioButton::~RadioButton() = default;

//--------------------------------------------------------------------------------------------------
QSize RadioButton::sizeHint() const
{
    QFontMetrics fm(font());

    int width = kRingSize;
    if (!text().isEmpty())
        width += kTextSpacing + fm.horizontalAdvance(text());

    return QSize(width, qMax(kMinTargetSize, fm.height()));
}

//--------------------------------------------------------------------------------------------------
QSize RadioButton::minimumSizeHint() const
{
    return sizeHint();
}

//--------------------------------------------------------------------------------------------------
void RadioButton::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled())
        painter.setOpacity(kDisabledOpacity);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);

    QRectF ring(0, 0, kRingSize, kRingSize);
    ring.moveTop(rect().center().y() - kRingSize / 2.0);
    if (rtl)
        ring.moveRight(rect().right());
    else
        ring.moveLeft(rect().left());

    const QColor accent = palette().color(QPalette::Highlight);
    const QColor outline = palette().color(QPalette::Mid);

    // State layer around the ring while the button is pressed.
    if (isDown())
    {
        QColor layer = accent;
        layer.setAlphaF(0.12);

        painter.setPen(Qt::NoPen);
        painter.setBrush(layer);
        painter.drawEllipse(ring.center(), kStateLayerSize / 2.0, kStateLayerSize / 2.0);
    }

    painter.setPen(QPen(Controls::blend(outline, accent, progress_), kRingWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ring.adjusted(kRingWidth / 2.0, kRingWidth / 2.0,
                                      -kRingWidth / 2.0, -kRingWidth / 2.0));

    // The inner dot grows from the ring center as the button turns on.
    const double dot_size = Controls::lerp(0.0, kDotSize, progress_);
    if (dot_size > 0.0)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawEllipse(ring.center(), dot_size / 2.0, dot_size / 2.0);
    }

    if (!text().isEmpty())
    {
        const QFontMetricsF fm(font());

        // The baseline is placed so the text is optically centered on the ring, ignoring the
        // line leading that Qt::AlignVCenter would add.
        const double baseline = ring.center().y() + (fm.ascent() - fm.descent()) / 2.0;
        const double text_x = rtl ? (ring.left() - kTextSpacing - fm.horizontalAdvance(text()))
                                  : (ring.right() + kTextSpacing);

        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QPointF(text_x, baseline), text());
    }
}

//--------------------------------------------------------------------------------------------------
bool RadioButton::hitButton(const QPoint& pos) const
{
    return rect().contains(pos);
}

//--------------------------------------------------------------------------------------------------
void RadioButton::onToggled(bool checked)
{
    animation_->stop();
    animation_->setStartValue(progress_);
    animation_->setEndValue(checked ? 1.0 : 0.0);
    animation_->start();
}
