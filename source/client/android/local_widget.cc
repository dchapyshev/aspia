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

#include "client/android/local_widget.h"

#include <QVBoxLayout>

#include "common/android/icon_button.h"
#include "common/android/label.h"

//--------------------------------------------------------------------------------------------------
LocalWidget::LocalWidget(QWidget* parent)
    : QWidget(parent),
      search_button_(new IconButton(":/img/material/search.svg", this)),
      placeholder_(new Label(QString(), Label::Role::CAPTION))
{
    // The action button lives in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so it does not linger in this widget.
    search_button_->hide();

    placeholder_->setAlignment(Qt::AlignCenter);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(placeholder_);

    retranslate();
}

//--------------------------------------------------------------------------------------------------
LocalWidget::~LocalWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> LocalWidget::appBarActions() const
{
    return { search_button_ };
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::retranslate()
{
    placeholder_->setText(tr("Local"));
}
