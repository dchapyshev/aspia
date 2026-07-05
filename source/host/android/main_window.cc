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

#include "host/android/main_window.h"

#include <QEvent>
#include <QVBoxLayout>

#include "base/logging.h"
#include "common/android/app_bar.h"
#include "common/android/label.h"

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::AndroidMainWindow(QWidget* parent)
    : QWidget(parent),
      app_bar_(new AppBar(this)),
      status_label_(new Label(QString(), Label::Role::BODY, this))
{
    LOG(INFO) << "Ctor";

    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(status_label_, 1);

    retranslate();
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
        retranslate();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    app_bar_->setTitle(tr("Aspia Host"));
}
