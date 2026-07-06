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

#include "common/android/scroll_area.h"

#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QShowEvent>

#include "common/android/scroll_indicator.h"

namespace {

constexpr int kFadeHeight = 24;

//--------------------------------------------------------------------------------------------------
// A gradient along the top or bottom edge: content fades into the background there instead of being cut
// off abruptly. Transparent to touch so scrolling passes through.
class FadeOverlay final : public QWidget
{
public:
    FadeOverlay(QWidget* parent, bool at_bottom)
        : QWidget(parent),
          at_bottom_(at_bottom)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent* /* event */) final
    {
        QColor opaque = palette().color(QPalette::Window);
        QColor transparent = opaque;
        transparent.setAlpha(0);

        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0.0, at_bottom_ ? transparent : opaque);
        gradient.setColorAt(1.0, at_bottom_ ? opaque : transparent);

        QPainter painter(this);
        painter.fillRect(rect(), gradient);
    }

private:
    const bool at_bottom_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
ScrollArea::ScrollArea(QWidget* parent)
    : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Qt on Android synthesizes mouse events from touches, so the scroller listens to the mouse
    // gesture: grabbing the touch gesture would swallow taps on the child controls.
    QScroller::grabGesture(viewport(), QScroller::LeftMouseButtonGesture);

    // The overshoot bounce makes a flick to an edge spring back instead of resting at it, so it
    // is disabled and the content stops cleanly at the top and bottom.
    QScroller* scroller = QScroller::scroller(viewport());
    QScrollerProperties properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                               QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                               QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(properties);

    top_fade_ = new FadeOverlay(this, false);
    bottom_fade_ = new FadeOverlay(this, true);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &ScrollArea::updateFades);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this]() { updateFades(); });

    // Created last so the scroll indicator stays above the fades at the corners.
    new ScrollIndicator(this);
}

//--------------------------------------------------------------------------------------------------
ScrollArea::~ScrollArea() = default;

//--------------------------------------------------------------------------------------------------
void ScrollArea::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);

    top_fade_->setGeometry(0, 0, width(), kFadeHeight);
    bottom_fade_->setGeometry(0, height() - kFadeHeight, width(), kFadeHeight);
    top_fade_->raise();
    bottom_fade_->raise();

    updateFades();
}

//--------------------------------------------------------------------------------------------------
void ScrollArea::showEvent(QShowEvent* event)
{
    QScrollArea::showEvent(event);
    updateFades();
}

//--------------------------------------------------------------------------------------------------
void ScrollArea::updateFades()
{
    const QScrollBar* scroll_bar = verticalScrollBar();

    top_fade_->setVisible(scroll_bar->value() > scroll_bar->minimum());
    bottom_fade_->setVisible(scroll_bar->value() < scroll_bar->maximum());

    top_fade_->update();
    bottom_fade_->update();
}
