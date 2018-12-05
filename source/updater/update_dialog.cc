//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "updater/update_dialog.h"

#include "build/version.h"
#include "ui_update_dialog.h"

namespace aspia {

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>()),
      checker_(this)
{
    ui->setupUi(this);

    ui->label_current->setText(QString("%1.%2.%3")
                               .arg(ASPIA_VERSION_MAJOR)
                               .arg(ASPIA_VERSION_MINOR)
                               .arg(ASPIA_VERSION_PATCH));

    checker_.checkForUpdates("");
}

UpdateDialog::~UpdateDialog() = default;

} // namespace aspia
