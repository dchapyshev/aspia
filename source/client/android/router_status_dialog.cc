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

#include "client/android/router_status_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "common/android/button.h"
#include "common/android/label.h"
#include "common/android/scroll_area.h"

namespace {

constexpr int kEventsMinHeight = 320;
constexpr int kIconSize = 20;
constexpr int kIconSpacing = 12;
constexpr int kRowSpacing = 12;

//--------------------------------------------------------------------------------------------------
QString iconForSeverity(RouterEvent::Severity severity)
{
    switch (severity)
    {
        case RouterEvent::Severity::WARNING:
            return ":/img/box-important.svg";

        case RouterEvent::Severity::CRITICAL:
            return ":/img/high-importance.svg";

        case RouterEvent::Severity::INFO:
        default:
            return ":/img/info.svg";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterStatusDialog::RouterStatusDialog(const QString& title, qint64 router_id,
                                       const QList<RouterEvent>& events, QWidget* parent)
    : Dialog(parent),
      scroll_(new ScrollArea(this)),
      router_id_(router_id)
{
    setTitle(title);

    QWidget* content = new QWidget(scroll_);
    events_layout_ = new QVBoxLayout(content);
    events_layout_->setContentsMargins(0, 0, 0, 0);
    events_layout_->setSpacing(kRowSpacing);
    events_layout_->addStretch();

    scroll_->setWidgetResizable(true);
    scroll_->setWidget(content);
    scroll_->setMinimumHeight(kEventsMinHeight);

    // The dialog card paints its own surface, so the scroll area and its viewport stay transparent
    // to avoid a mismatched rectangle behind the events.
    scroll_->setStyleSheet("background: transparent;");
    scroll_->viewport()->setStyleSheet("background: transparent;");

    contentLayout()->addWidget(scroll_);

    for (const RouterEvent& event : events)
        appendEvent(event);

    Button* clear = addButton(tr("Clear"), Button::Role::TEXT);
    connect(clear, &Button::clicked, this, &RouterStatusDialog::onClear);

    Button* close = addButton(tr("Close"), Button::Role::FILLED);
    connect(close, &Button::clicked, this, &RouterStatusDialog::accept);
}

//--------------------------------------------------------------------------------------------------
RouterStatusDialog::~RouterStatusDialog() = default;

//--------------------------------------------------------------------------------------------------
void RouterStatusDialog::onRouterEvent(qint64 router_id, const RouterEvent& event)
{
    if (router_id == router_id_)
        appendEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusDialog::onClear()
{
    // Remove every row, keeping the trailing stretch that holds them at the top.
    while (events_layout_->count() > 1)
    {
        QLayoutItem* item = events_layout_->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    emit sig_clearEvents(router_id_);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusDialog::appendEvent(const RouterEvent& event)
{
    QWidget* row = new QWidget();

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kIconSpacing);

    QPixmap pixmap = GuiApplication::svgPixmap(iconForSeverity(event.severity),
                                               QSize(kIconSize, kIconSize));

    QLabel* icon = new QLabel(row);
    icon->setFixedSize(kIconSize, kIconSize);
    icon->setPixmap(pixmap);
    layout->addWidget(icon, 0, Qt::AlignTop);

    Label* text = new Label(QString("%1  %2").arg(event.time.toString("HH:mm:ss"), event.text),
                            Label::Role::BODY, row);
    text->setWordWrap(true);
    layout->addWidget(text, 1);

    // Keep the trailing stretch last so the rows stay top-aligned.
    events_layout_->insertWidget(events_layout_->count() - 1, row);

    scrollToBottom();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusDialog::scrollToBottom()
{
    // The layout has not been recomputed yet, so defer until the new row has a geometry.
    QScrollBar* bar = scroll_->verticalScrollBar();
    QMetaObject::invokeMethod(this, [bar]() { bar->setValue(bar->maximum()); },
                              Qt::QueuedConnection);
}
