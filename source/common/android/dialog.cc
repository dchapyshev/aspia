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

#include "common/android/dialog.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "common/android/label.h"

namespace {

constexpr int kCornerRadius = 16;
constexpr int kMaxCardWidth = 460;
constexpr int kScreenMargin = 40;
constexpr int kCardPadding = 24;
constexpr int kCardSpacing = 16;
constexpr int kButtonSpacing = 8;
constexpr int kScrimAlpha = 82;

// Rounded surface holding the dialog content.
class Card final : public QWidget
{
public:
    explicit Card(QWidget* parent)
        : QWidget(parent)
    {
        // Nothing.
    }

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
    }
};

} // namespace

//--------------------------------------------------------------------------------------------------
Dialog::Dialog(QWidget* parent)
    : QDialog(parent),
      card_(new Card(this)),
      title_label_(new Label(QString(), Label::Role::TITLE, card_)),
      text_label_(new Label(QString(), Label::Role::BODY, card_)),
      content_layout_(new QVBoxLayout()),
      button_row_(new QHBoxLayout())
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    title_label_->setWordWrap(true);
    title_label_->setVisible(false);
    text_label_->setWordWrap(true);
    text_label_->setVisible(false);

    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(kCardSpacing);

    button_row_->setContentsMargins(0, 0, 0, 0);
    button_row_->setSpacing(kButtonSpacing);
    button_row_->addStretch();

    QVBoxLayout* card_layout = new QVBoxLayout(card_);
    card_layout->setContentsMargins(kCardPadding, kCardPadding, kCardPadding, kCardPadding);
    card_layout->setSpacing(kCardSpacing);
    card_layout->addWidget(title_label_);
    card_layout->addWidget(text_label_);
    card_layout->addLayout(content_layout_);
    card_layout->addLayout(button_row_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kScreenMargin, kScreenMargin, kScreenMargin, kScreenMargin);
    layout->addWidget(card_, 0, Qt::AlignCenter);
}

//--------------------------------------------------------------------------------------------------
Dialog::~Dialog() = default;

//--------------------------------------------------------------------------------------------------
void Dialog::setTitle(const QString& title)
{
    title_label_->setText(title);
    title_label_->setVisible(!title.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void Dialog::setText(const QString& text)
{
    text_label_->setText(text);
    text_label_->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------------------------------
Button* Dialog::addButton(const QString& text, Button::Role role)
{
    Button* button = new Button(text, role, card_);

    // The primary (filled) action answers the Enter key; the other buttons stay out of the way so
    // they are not triggered as the default.
    const bool primary = (role == Button::Role::FILLED);
    button->setAutoDefault(primary);
    button->setDefault(primary);

    button_row_->addWidget(button);
    return button;
}

//--------------------------------------------------------------------------------------------------
void Dialog::done(int result)
{
    if (QWidget* focus = focusWidget())
        focus->clearFocus();

    QGuiApplication::inputMethod()->hide();
    QDialog::done(result);
}

//--------------------------------------------------------------------------------------------------
void Dialog::mousePressEvent(QMouseEvent* event)
{
    QDialog::mousePressEvent(event);

    // A tap on the scrim outside the card dismisses the dialog.
    if (!card_->geometry().contains(event->position().toPoint()))
        reject();
}

//--------------------------------------------------------------------------------------------------
void Dialog::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, kScrimAlpha));
}

//--------------------------------------------------------------------------------------------------
void Dialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateCardWidth();
}

//--------------------------------------------------------------------------------------------------
void Dialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // The dialog covers the whole parent window, so the scrim dims everything behind the card.
    if (parentWidget())
        setGeometry(parentWidget()->window()->geometry());

    // Constrain the card to the final geometry: setGeometry above does not always trigger a resize
    // event, which would otherwise leave the card at its content width and overflow a narrow screen.
    updateCardWidth();
}

//--------------------------------------------------------------------------------------------------
void Dialog::updateCardWidth()
{
    // The dialog is a native window on Android whose own width may be sized to its content, so the
    // card is capped against the parent window (the actual screen) instead.
    int available = width();
    if (parentWidget() && parentWidget()->window())
        available = parentWidget()->window()->width();

    card_->setFixedWidth(qMin(kMaxCardWidth, available - kScreenMargin * 2));
}
