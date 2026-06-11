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

#include <QScroller>
#include <QScrollerProperties>

#include "common/android/scroll_indicator.h"

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

    new ScrollIndicator(this);
}

//--------------------------------------------------------------------------------------------------
ScrollArea::~ScrollArea() = default;
