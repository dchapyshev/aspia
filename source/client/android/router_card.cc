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

#include "client/android/router_card.h"

#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <functional>

#include "base/gui_application.h"
#include "common/android/card.h"
#include "common/android/controls.h"
#include "common/android/label.h"

namespace {

constexpr int kStatusIconSize = 24;
constexpr int kContentHMargin = 13;
constexpr int kEventIconSize = 20;
constexpr int kEventIconSpacing = 12;
constexpr int kEventRowSpacing = 12;
constexpr int kEventRowHintWidth = 600;
constexpr int kMaxEvents = 7;

//--------------------------------------------------------------------------------------------------
QString statusIconPath(Router::Status status)
{
    switch (status)
    {
        case Router::Status::CONNECTING:
            return ":/img/router-connecting.svg";

        case Router::Status::ONLINE:
            return ":/img/router-online.svg";

        case Router::Status::OFFLINE:
        default:
            return ":/img/router-offline.svg";
    }
}

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

// A single event line: a severity icon and word-wrapped text. The row measures its own height from
// its current width with QFontMetrics, which stays correct at any display density where a wrapped
// QLabel's height-for-width is not reliable, then pins that height and reports the change so the
// owner can grow the panel to fit. The row is painted directly to avoid a nested layout.
class EventRow final : public QWidget
{
public:
    EventRow(const QPixmap& icon, const QString& text, QWidget* parent = nullptr)
        : QWidget(parent),
          icon_(icon),
          text_(text)
    {
        setFont(Controls::scaledFont(font(), Controls::kFontScale));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    // Invoked after the row pins a new height so the owner can resize the panel around it.
    void setHeightChangedCallback(std::function<void()> callback)
    {
        height_changed_ = std::move(callback);
    }

    QSize sizeHint() const final
    {
        return QSize(kEventIconSize, textHeight(width() > 0 ? width() : kEventRowHintWidth));
    }

    QSize minimumSizeHint() const final
    {
        return QSize(kEventIconSize, kEventIconSize);
    }

protected:
    void resizeEvent(QResizeEvent* /* event */) final
    {
        // The real width is known only once laid out; recompute the wrapped height for it and pin
        // it so the layout reserves the full text instead of squeezing it to a single line.
        const int height = textHeight(width());
        if (height == height_)
            return;

        height_ = height;
        setFixedHeight(height);
        if (height_changed_)
            height_changed_();
    }

    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);
        painter.drawPixmap(0, 0, icon_);

        const int text_x = kEventIconSize + kEventIconSpacing;
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QRect(text_x, 0, width() - text_x, height()),
                         Qt::TextWordWrap | Qt::AlignTop, text_);
    }

private:
    int textHeight(int width) const
    {
        const int text_width = qMax(0, width - kEventIconSize - kEventIconSpacing);
        const int text_height = fontMetrics().boundingRect(
            QRect(0, 0, text_width, 0), Qt::TextWordWrap, text_).height();
        return qMax(kEventIconSize, text_height);
    }

    QPixmap icon_;
    QString text_;
    std::function<void()> height_changed_;
    int height_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterCard::RouterCard(qint64 router_id, const QString& name, QWidget* parent)
    : ExpandablePanel(parent),
      router_id_(router_id)
{
    // Header: status icon and name.
    status_icon_ = new QLabel();
    status_icon_->setFixedSize(kStatusIconSize, kStatusIconSize);
    headerLayout()->addWidget(status_icon_);

    name_label_ = new Label(name, Label::Role::BODY);
    headerLayout()->addWidget(name_label_, 1);

    // Panel: the connection event log on a lifted surface.
    contentLayout()->setContentsMargins(kContentHMargin, 0, kContentHMargin, kContentHMargin);

    Card* card = new Card(Card::Role::FILLED);

    QWidget* events_container = new QWidget(card);
    events_layout_ = new QVBoxLayout(events_container);
    events_layout_->setContentsMargins(0, 0, 0, 0);
    events_layout_->setSpacing(kEventRowSpacing);
    card->contentLayout()->addWidget(events_container);

    contentLayout()->addWidget(card);

    connect(this, &ExpandablePanel::sig_headerClicked, this, [this]() { emit expandRequested(router_id_); });
    connect(this, &ExpandablePanel::sig_headerLongPressed, this, [this]() { emit editRequested(router_id_); });
}

//--------------------------------------------------------------------------------------------------
RouterCard::~RouterCard() = default;

//--------------------------------------------------------------------------------------------------
void RouterCard::setName(const QString& name)
{
    name_label_->setText(name);
}

//--------------------------------------------------------------------------------------------------
void RouterCard::setStatus(Router::Status status)
{
    status_icon_->setPixmap(GuiApplication::svgPixmap(statusIconPath(status),
                                                      QSize(kStatusIconSize, kStatusIconSize)));
}

//--------------------------------------------------------------------------------------------------
void RouterCard::setEvents(const QList<RouterEvent>& events)
{
    clearEvents();

    for (const RouterEvent& event : events)
        appendEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterCard::appendEvent(const RouterEvent& event)
{
    const QPixmap icon = GuiApplication::svgPixmap(iconForSeverity(event.severity),
                                                  QSize(kEventIconSize, kEventIconSize));
    const QString text = QString("%1  %2").arg(event.time.toString("HH:mm:ss"), event.text);

    EventRow* row = new EventRow(icon, text);
    row->setHeightChangedCallback([this]()
    {
        // The row pins its height from inside its own resizeEvent, which runs during the layout
        // pass; re-pinning the panel inline would read stale cached sizes. Defer it so the panel
        // grows once the pass has settled.
        QMetaObject::invokeMethod(this, [this]() { contentChanged(); }, Qt::QueuedConnection);
    });
    // Newest events are shown first, so each new row goes to the top.
    events_layout_->insertWidget(0, row);

    // Keep only the most recent events on screen, dropping the oldest rows beyond the limit.
    while (events_layout_->count() > kMaxEvents)
    {
        QLayoutItem* oldest = events_layout_->takeAt(events_layout_->count() - 1);
        if (QWidget* widget = oldest->widget())
            widget->deleteLater();
        delete oldest;
    }

    // Grow the open card to fit the new event.
    contentChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterCard::clearEvents()
{
    QLayoutItem* item;
    while ((item = events_layout_->takeAt(0)) != nullptr)
    {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    // Shrink the card back if its panel is open.
    contentChanged();
}
