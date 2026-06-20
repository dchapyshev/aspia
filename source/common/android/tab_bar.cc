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

#include "common/android/tab_bar.h"

#include <QMouseEvent>
#include <QPainter>

#include <utility>

#include "common/android/controls.h"

namespace {

constexpr int kHeight = 52;
constexpr int kHorizontalPadding = 16;
constexpr int kMinTabWidth = 80;
constexpr int kIndicatorHeight = 3;
constexpr double kDividerWidth = 1.0;
constexpr double kInactiveTextOpacity = 0.6;
constexpr double kPressLayerOpacity = 0.08;

} // namespace

//--------------------------------------------------------------------------------------------------
TabBar::TabBar(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(Controls::scaledFont(font(), Controls::kFontScale));
}

//--------------------------------------------------------------------------------------------------
TabBar::~TabBar() = default;

//--------------------------------------------------------------------------------------------------
void TabBar::addTab(const QString& text)
{
    tabs_.append(text);
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------------------------------
void TabBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= tabs_.size() || index == current_)
        return;

    current_ = index;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize TabBar::sizeHint() const
{
    QFontMetrics metrics(font());

    int width = 0;
    for (const QString& text : std::as_const(tabs_))
        width += qMax(kMinTabWidth, metrics.horizontalAdvance(text) + 2 * kHorizontalPadding);

    return QSize(width, kHeight);
}

//--------------------------------------------------------------------------------------------------
void TabBar::paintEvent(QPaintEvent* /* event */)
{
    if (tabs_.isEmpty())
        return;

    QPainter painter(this);

    const QRectF bounds = rect();
    const QColor accent = Controls::accentColor();
    const int count = tabs_.size();
    const double tab_width = bounds.width() / count;

    // The top divider sets the bar off from the content above it.
    painter.setPen(QPen(palette().color(QPalette::Mid), kDividerWidth));
    painter.drawLine(QPointF(bounds.left(), bounds.top() + kDividerWidth / 2.0),
                     QPointF(bounds.right(), bounds.top() + kDividerWidth / 2.0));

    painter.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < count; ++i)
    {
        const QRectF tab(bounds.left() + i * tab_width, bounds.top(), tab_width, bounds.height());
        const bool active = (i == current_);

        if (i == pressed_)
        {
            QColor layer = accent;
            layer.setAlphaF(kPressLayerOpacity);
            painter.fillRect(tab, layer);
        }

        // The active tab carries an accent indicator along the top edge, pointing at the content.
        if (active)
            painter.fillRect(QRectF(tab.left(), tab.top(), tab.width(), kIndicatorHeight), accent);

        QColor text_color = accent;
        if (!active)
        {
            text_color = palette().color(QPalette::WindowText);
            text_color.setAlphaF(kInactiveTextOpacity);
        }

        QFont label_font = font();
        label_font.setWeight(active ? QFont::Medium : QFont::Normal);
        painter.setFont(label_font);
        painter.setPen(text_color);

        const QString text = painter.fontMetrics().elidedText(
            tabs_[i], Qt::ElideRight, int(tab_width - 2 * kHorizontalPadding));
        painter.drawText(tab, Qt::AlignCenter, text);
    }
}

//--------------------------------------------------------------------------------------------------
void TabBar::mousePressEvent(QMouseEvent* event)
{
    pressed_ = tabAt(event->position().toPoint());
    update();
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    const int tab = tabAt(event->position().toPoint());
    pressed_ = -1;

    if (tab >= 0 && tab != current_)
    {
        current_ = tab;
        emit sig_currentChanged(current_);
    }

    update();
}

//--------------------------------------------------------------------------------------------------
int TabBar::tabAt(const QPoint& pos) const
{
    if (tabs_.isEmpty() || !rect().contains(pos))
        return -1;

    const int index = pos.x() * tabs_.size() / width();
    return qBound(0, index, static_cast<int>(tabs_.size()) - 1);
}
