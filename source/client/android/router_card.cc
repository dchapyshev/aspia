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

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "client/router/router_controller.h"
#include "common/android/button.h"
#include "common/android/card.h"
#include "common/android/controls.h"
#include "common/android/label.h"

namespace {

constexpr int kHeaderHeight = 56;
constexpr int kHeaderSpacing = 12;
constexpr int kStatusIconSize = 24;
constexpr int kContentHMargin = 13;
constexpr int kEventIconSize = 20;
constexpr int kEventIconSpacing = 12;
constexpr int kEventRowSpacing = 12;
constexpr int kEventRowHintWidth = 600;

//--------------------------------------------------------------------------------------------------
QString statusIconPath(RouterStatus status)
{
    switch (status)
    {
        case RouterStatus::CONNECTING:
            return ":/img/router-connecting.svg";

        case RouterStatus::ONLINE:
            return ":/img/router-online.svg";

        case RouterStatus::TWO_FACTOR:
            return ":/img/router-lock.svg";

        case RouterStatus::UNREADABLE:
            return ":/img/router-warning.svg";

        case RouterStatus::OFFLINE:
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
// QLabel's height-for-width is not reliable, then pins that height. The row is painted directly to
// avoid a nested layout.
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
    int height_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterCard::RouterCard(const QString& name, QWidget* parent)
    : QWidget(parent)
{
    QWidget* header = new QWidget(this);
    header->setFixedHeight(kHeaderHeight);

    QHBoxLayout* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(kHeaderSpacing);

    label_status_icon_ = new QLabel();
    label_status_icon_->setFixedSize(kStatusIconSize, kStatusIconSize);
    header_layout->addWidget(label_status_icon_);

    label_name_ = new Label(name, Label::Role::BODY);
    header_layout->addWidget(label_name_, 1);

    // The way to answer the two-factor stage: shown only while the session stands at it.
    button_two_factor_ = new Button(tr("Enter Code"), Button::Role::FILLED);
    button_two_factor_->setVisible(false);
    connect(button_two_factor_, &Button::clicked, this, &RouterCard::sig_twoFactorClicked);

    // The connection event log on a lifted surface.
    events_card_ = new Card(Card::Role::FILLED);
    events_card_->setVisible(false);

    QWidget* events_container = new QWidget(events_card_);
    events_layout_ = new QVBoxLayout(events_container);
    events_layout_->setContentsMargins(0, 0, 0, 0);
    events_layout_->setSpacing(kEventRowSpacing);
    events_card_->contentLayout()->addWidget(events_container);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kContentHMargin, 0, kContentHMargin, kContentHMargin);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(button_two_factor_);
    layout->addWidget(events_card_);
}

//--------------------------------------------------------------------------------------------------
RouterCard::~RouterCard() = default;


//--------------------------------------------------------------------------------------------------
void RouterCard::setStatus(RouterStatus status)
{
    label_status_icon_->setPixmap(GuiApplication::svgPixmap(statusIconPath(status),
                                                      QSize(kStatusIconSize, kStatusIconSize)));
    updateTwoFactorButton();
}

//--------------------------------------------------------------------------------------------------
void RouterCard::updateTwoFactorButton()
{
    // The router does not look at codes while the block runs, so the button is disabled for it,
    // but it stays on the card. A hidden button would read as if no question existed, and the
    // journal line naming the wait can be pushed out by the reconnect noise of the blocked stage.
    TwoFactorPrompt* prompt = RouterController::twoFactorPrompt();
    button_two_factor_->setVisible(prompt != nullptr);

    if (prompt)
    {
        const bool blocked = prompt->blockedSeconds() > 0;
        button_two_factor_->setEnabled(!blocked);

        // An account with no secret yet is walked through the enrollment first, and the button says so.
        if (blocked)
            button_two_factor_->setText(tr("Blocked"));
        else
            button_two_factor_->setText(prompt->otpauthUri().isEmpty() ? tr("Enter Code") : tr("Set Up"));
    }
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

    // Newest events are shown first, so each new row goes to the top.
    events_layout_->insertWidget(0, new EventRow(icon, text));
    events_card_->setVisible(true);

    // The journal keeps a limited number of events, and so does the screen.
    while (events_layout_->count() > RouterController::kMaxStoredEvents)
    {
        QLayoutItem* oldest = events_layout_->takeAt(events_layout_->count() - 1);
        if (QWidget* widget = oldest->widget())
            widget->deleteLater();
        delete oldest;
    }
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

    events_card_->setVisible(false);
}
