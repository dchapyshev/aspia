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

#include "client/desktop/management/router_status_widget.h"

#include <QTreeWidget>

#include <gtest/gtest.h>

#include "client/router_controller.h"

//--------------------------------------------------------------------------------------------------
// The tree of the journal is user-sortable, so the cap must drop the oldest event and not
// whatever the current order puts first. Sorted newest-first, the fresh row lands at index 0,
// exactly where a cap by index would delete it.
TEST(RouterStatusWidgetTest, CapDropsTheOldestEventUnderAnySortOrder)
{
    // The widget reaches the controller for the prompt state; there is no worker in this stand.
    RouterController controller;

    RouterStatusWidget widget;

    QList<RouterEvent> events;
    for (int i = 0; i < RouterController::kMaxStoredEvents; ++i)
    {
        events.append(RouterEvent{ QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0)).addSecs(i * 60),
                                   RouterEvent::Severity::INFO, QString("event %1").arg(i) });
    }
    widget.showRouter(1, events);

    auto* tree = widget.findChild<QTreeWidget*>("tree_events");
    ASSERT_TRUE(tree);
    ASSERT_EQ(tree->topLevelItemCount(), RouterController::kMaxStoredEvents);

    tree->sortItems(0, Qt::DescendingOrder);

    widget.onEvent(1, RouterEvent{ QDateTime(QDate(2026, 1, 2), QTime(12, 0, 0)),
                                   RouterEvent::Severity::INFO, QString("fresh") });

    EXPECT_EQ(tree->topLevelItemCount(), RouterController::kMaxStoredEvents);

    bool fresh_found = false;
    bool oldest_found = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        const QString text = tree->topLevelItem(i)->text(1);
        if (text == QString("fresh"))
            fresh_found = true;
        else if (text == QString("event 0"))
            oldest_found = true;
    }

    EXPECT_TRUE(fresh_found);
    EXPECT_FALSE(oldest_found);
}
