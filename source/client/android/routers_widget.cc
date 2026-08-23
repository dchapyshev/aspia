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

#include "client/android/routers_widget.h"

#include <QGridLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "client/android/router_editor.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/scroll_area.h"

namespace {

constexpr double kPlaceholderTextOpacity = 0.6;

} // namespace

// Overlay shown over the empty list: the standard window surface with a centered hint.
class RoutersEmptyView final : public QWidget
{
public:
    explicit RoutersEmptyView(QWidget* parent)
        : QWidget(parent)
    {
        // Nothing.
    }

    void setText(const QString& text)
    {
        text_ = text;
        update();
    }

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);

        painter.fillRect(rect(), palette().color(QPalette::Window));

        QColor text_color = palette().color(QPalette::WindowText);
        text_color.setAlphaF(kPlaceholderTextOpacity);
        painter.setPen(text_color);
        painter.setFont(Controls::scaledFont(font(), Controls::kFontScale));
        painter.drawText(rect(), Qt::AlignCenter, text_);
    }

private:
    QString text_;
};

//--------------------------------------------------------------------------------------------------
RoutersWidget::RoutersWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      scroll_(new ScrollArea(this)),
      container_(new QWidget(scroll_)),
      cards_layout_(new QVBoxLayout(container_)),
      placeholder_(new RoutersEmptyView(this)),
      editor_(new RouterEditor(this)),
      add_button_(new IconButton(":/img/material/add_2.svg", this))
{
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    cards_layout_->setSpacing(0);
    cards_layout_->addStretch();

    scroll_->setWidget(container_);

    // The add action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    add_button_->hide();

    // List page: the scrollable cards with the empty-state hint overlaid on top.
    QWidget* list_page = new QWidget(stack_);
    QGridLayout* list_layout = new QGridLayout(list_page);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->addWidget(scroll_, 0, 0);
    list_layout->addWidget(placeholder_, 0, 0);
    placeholder_->raise();
    placeholder_->hide();

    stack_->addWidget(list_page);
    stack_->addWidget(editor_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(add_button_, &IconButton::clicked, this, &RoutersWidget::onAddRouter);
    connect(editor_, &RouterEditor::sig_accepted, this, &RoutersWidget::returnFromEditor);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (RouterCard* card = cards_.value(router_id))
            card->setStatus(status);
    });
    connect(&controller, &RouterController::sig_event, this, &RoutersWidget::onRouterEvent);

    placeholder_->setText(tr("No routers added"));
    reload();
}

//--------------------------------------------------------------------------------------------------
RoutersWidget::~RoutersWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RoutersWidget::appBarActions() const
{
    // The editor screen has its own form; no list actions there.
    if (isEditorPage())
        return {};

    return { add_button_ };
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::reload()
{
    clearCards();

    // The router fields are decrypted with the master-password-derived key, so there is nothing to
    // show until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
    {
        cards_layout_->addStretch();
        placeholder_->hide();
        emit sig_appBarActionsChanged();
        return;
    }

    RouterController::instance().reload();

    for (const RouterConfig& config : Database::instance().routerList())
    {
        const qint64 router_id = config.routerId();

        // The event log is filled lazily when the panel is opened, so the card starts empty.
        RouterCard* card = new RouterCard(router_id, config.displayLabel(), container_);
        card->setStatus(RouterController::status(router_id));

        connect(card, &RouterCard::sig_expandRequested, this, &RoutersWidget::onCardExpandRequested);
        connect(card, &RouterCard::sig_editRequested, this, &RoutersWidget::onEditRouter);
        connect(card, &RouterCard::sig_twoFactorClicked, this, &RoutersWidget::sig_twoFactorClicked);

        cards_layout_->addWidget(card);
        cards_.insert(router_id, card);
    }

    cards_layout_->addStretch();

    placeholder_->setVisible(cards_.isEmpty());
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::goBack()
{
    showList();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::resetToList()
{
    if (isEditorPage())
        stack_->setCurrentIndex(0);
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onAddRouter()
{
    editor_->prepareForAdd();
    stack_->setCurrentIndex(1);
    emit sig_titleChanged(tr("Add Router"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onCardExpandRequested(qint64 router_id)
{
    // Tapping the open router closes its panel; tapping another closes the previous one first, so
    // only a single panel is ever open.
    if (expanded_router_id_ == router_id)
    {
        if (RouterCard* card = cards_.value(router_id))
            card->setExpanded(false);
        expanded_router_id_ = -1;
        return;
    }

    if (RouterCard* previous = cards_.value(expanded_router_id_))
        previous->setExpanded(false);

    RouterCard* card = cards_.value(router_id);
    if (!card)
        return;

    // Fill the log from the stored history only now that the panel is being shown.
    card->setEvents(RouterController::instance().events(router_id));
    card->setExpanded(true);
    expanded_router_id_ = router_id;
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onEditRouter(qint64 router_id)
{
    if (!editor_->prepareForEdit(router_id))
        return;

    stack_->setCurrentIndex(1);
    emit sig_titleChanged(tr("Edit Router"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onRouterEvent(qint64 router_id, const RouterEvent& event)
{
    RouterCard* card = cards_.value(router_id);
    if (!card)
        return;

    card->updateTwoFactorButton();

    // Only the open panel shows its log live; the others are refilled from the journal when
    // expanded.
    if (router_id != expanded_router_id_)
        return;

    card->appendEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::returnFromEditor()
{
    // The router list changed (added, edited or removed), so it is rebuilt before returning.
    reload();
    showList();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::showList()
{
    stack_->setCurrentIndex(0);
    emit sig_titleChanged(QString(), false);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
bool RoutersWidget::isEditorPage() const
{
    return stack_->currentIndex() == 1;
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::clearCards()
{
    QLayoutItem* item;
    while ((item = cards_layout_->takeAt(0)) != nullptr)
    {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    cards_.clear();
    expanded_router_id_ = -1;
}
