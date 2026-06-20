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

#include "client/android/file_progress_sheet.h"

#include <QProgressBar>
#include <QVBoxLayout>

#include "common/android/button.h"
#include "common/android/label.h"

//--------------------------------------------------------------------------------------------------
FileProgressSheet::FileProgressSheet(const QString& title, QWidget* parent)
    : Dialog(parent),
      item_(new Label(QString(), Label::Role::CAPTION, this)),
      bar_(new QProgressBar(this))
{
    setTitle(title);

    item_->setWordWrap(true);

    bar_->setRange(0, 100);
    bar_->setTextVisible(false);

    contentLayout()->addWidget(item_);
    contentLayout()->addWidget(bar_);

    Button* cancel = addButton(tr("Cancel"), Button::Role::TEXT);
    connect(cancel, &Button::clicked, this, [this]()
    {
        emit sig_cancel();
    });
}

//--------------------------------------------------------------------------------------------------
FileProgressSheet::~FileProgressSheet() = default;

//--------------------------------------------------------------------------------------------------
void FileProgressSheet::setCurrentItem(const QString& text)
{
    item_->setText(text);
}

//--------------------------------------------------------------------------------------------------
void FileProgressSheet::setProgress(int percentage)
{
    bar_->setValue(percentage);
}
