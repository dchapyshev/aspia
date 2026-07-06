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

#include "client/desktop/hosts/search_dialog.h"

#include <QIcon>
#include <QLineEdit>
#include <QSignalBlocker>

#include "base/logging.h"
#include "ui_search_dialog.h"

//--------------------------------------------------------------------------------------------------
SearchDialog::SearchDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::SearchDialog>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->edit_search->setClearButtonEnabled(true);
    ui->edit_search->addAction(QIcon(":/img/search.svg"), QLineEdit::LeadingPosition);

    connect(ui->edit_search, &QLineEdit::textChanged, this, &SearchDialog::sig_searchTextChanged);

    setFixedHeight(sizeHint().height());
}

//--------------------------------------------------------------------------------------------------
SearchDialog::~SearchDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SearchDialog::setSearchText(const QString& text)
{
    // Seed the field without triggering a fresh search.
    QSignalBlocker blocker(ui->edit_search);
    ui->edit_search->setText(text);
}

//--------------------------------------------------------------------------------------------------
void SearchDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    ui->edit_search->setFocus();
    ui->edit_search->selectAll();
}
