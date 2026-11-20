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

#include "client/android/router_widget.h"

#include <QGridLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "client/config.h"
#include "client/android/router_editor.h"
#include "client/router/router_controller.h"
#include "client/storage/router_store.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/scroll_area.h"

namespace {

constexpr double kPlaceholderTextOpacity = 0.6;

} // namespace

// Overlay shown over the empty list: the standard window surface with a centered hint.
class RouterEmptyView final : public QWidget
{
public:
    explicit RouterEmptyView(QWidget* parent)
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
RouterWidget::RouterWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      scroll_(new ScrollArea(this)),
      container_(new QWidget(scroll_)),
      cards_layout_(new QVBoxLayout(container_)),
      placeholder_(new RouterEmptyView(this)),
      editor_(new RouterEditor(this)),
      button_add_(new IconButton(":/img/material/add_2.svg", this)),
      button_clear_(new IconButton(":/img/material/mop.svg", this)),
      button_edit_(new IconButton(":/img/material/edit.svg", this))
{
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    cards_layout_->setSpacing(0);
    cards_layout_->addStretch();

    scroll_->setWidget(container_);

    // The actions live in the app bar; AppBar::setActions() reparents and shows them. Hidden by
    // default so they do not linger in this widget.
    button_add_->hide();
    button_clear_->hide();
    button_edit_->hide();

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

    connect(button_add_, &IconButton::clicked, this, &RouterWidget::onAddRouter);
    connect(button_clear_, &IconButton::clicked, this, &RouterWidget::onClearEvents);
    connect(button_edit_, &IconButton::clicked, this, &RouterWidget::onEditRouter);
    connect(editor_, &RouterEditor::sig_accepted, this, &RouterWidget::returnFromEditor);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](RouterStatus status)
    {
        if (card_)
            card_->setStatus(status);
    });
    connect(&controller, &RouterController::sig_event, this, &RouterWidget::onRouterEvent);

    placeholder_->setText(tr("No router is set"));
    reload();
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RouterWidget::appBarActions() const
{
    // The editor screen has its own form; no list actions there.
    if (isEditorPage())
        return {};

    if (!card_)
        return { button_add_ };

    return { button_clear_, button_edit_ };
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::reload()
{
    removeCard();

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

    RouterConfig config;
    const Database::FindResult found = RouterStore().find(&config);
    if (found == Database::FindResult::FAILED)
        LOG(ERROR) << "Unable to read the router";

    if (found == Database::FindResult::FOUND || found == Database::FindResult::UNREADABLE)
    {
        const QString label = config.address().isEmpty() ? tr("Router") : config.address();

        card_ = new RouterCard(config.isEnabled() ? label : tr("%1 (disabled)").arg(label), container_);
        card_->setStatus(RouterController::status());
        card_->setEvents(RouterController::instance().events());

        connect(card_, &RouterCard::sig_twoFactorClicked, this, &RouterWidget::sig_twoFactorClicked);

        cards_layout_->addWidget(card_);
    }

    cards_layout_->addStretch();

    placeholder_->setVisible(!card_);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::goBack()
{
    showList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::resetToList()
{
    if (isEditorPage())
        stack_->setCurrentIndex(0);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onAddRouter()
{
    editor_->prepareForAdd();
    stack_->setCurrentIndex(1);
    emit sig_titleChanged(tr("Add Router"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onClearEvents()
{
    if (!card_)
        return;

    RouterController& controller = RouterController::instance();
    controller.clearEvents();
    card_->setEvents(controller.events());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onEditRouter()
{
    if (!editor_->prepareForEdit())
        return;

    stack_->setCurrentIndex(1);
    emit sig_titleChanged(tr("Edit Router"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRouterEvent(const RouterEvent& event)
{
    if (!card_)
        return;

    card_->updateTwoFactorButton();
    card_->appendEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::returnFromEditor()
{
    // The router was added or edited, so the card is rebuilt before returning.
    reload();
    showList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::showList()
{
    stack_->setCurrentIndex(0);
    emit sig_titleChanged(QString(), false);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::isEditorPage() const
{
    return stack_->currentIndex() == 1;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::removeCard()
{
    QLayoutItem* item;
    while ((item = cards_layout_->takeAt(0)) != nullptr)
    {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    card_ = nullptr;
}
