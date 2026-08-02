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

#include "client/desktop/status_overlay.h"

#include <QEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
StatusOverlay::StatusOverlay(QWidget* parent)
    : QWidget(parent)
{
    CHECK(parent);

    setAutoFillBackground(true);

    message_label_ = new QLabel(this);
    message_label_->setAlignment(Qt::AlignCenter);
    message_label_->setWordWrap(true);

    busy_bar_ = new QProgressBar(this);
    busy_bar_->setRange(0, 0);
    busy_bar_->setTextVisible(false);
    busy_bar_->setFixedWidth(240);

    close_button_ = new QPushButton(tr("Close"), this);
    connect(close_button_, &QPushButton::clicked, this, &StatusOverlay::sig_closeRequested);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addStretch();
    layout->addWidget(message_label_);
    layout->addWidget(busy_bar_, 0, Qt::AlignHCenter);
    layout->addWidget(close_button_, 0, Qt::AlignHCenter);
    layout->addStretch();

    // Track the parent to always cover it entirely and stay on top of siblings created later.
    parent->installEventFilter(this);
    setGeometry(parent->rect());
}

//--------------------------------------------------------------------------------------------------
StatusOverlay::~StatusOverlay() = default;

//--------------------------------------------------------------------------------------------------
void StatusOverlay::setProgress(const QString& message)
{
    showMessage(message, true);
}

//--------------------------------------------------------------------------------------------------
void StatusOverlay::setError(const QString& message)
{
    showMessage(message, false);
}

//--------------------------------------------------------------------------------------------------
bool StatusOverlay::eventFilter(QObject* object, QEvent* event)
{
    if (object == parentWidget())
    {
        if (event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        else if (event->type() == QEvent::Show)
            raise();
    }

    return QWidget::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void StatusOverlay::showMessage(const QString& message, bool busy)
{
    message_label_->setText(message);
    busy_bar_->setVisible(busy);
    show();
    raise();
}
