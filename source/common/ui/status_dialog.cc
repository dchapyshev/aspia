//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/status_dialog.h"

#include "base/logging.h"
#include "ui_status_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QTime>

namespace common {

//--------------------------------------------------------------------------------------------------
StatusDialog::StatusDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::StatusDialog>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    QPushButton* close_button = ui->buttonbox->button(QDialogButtonBox::StandardButton::Close);
    if (close_button)
        close_button->setText(tr("Close"));

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button)
    {
        if (ui->buttonbox->standardButton(button) == QDialogButtonBox::Close)
        {
            LOG(INFO) << "[ACTION] Close button clicked";
            close();
        }
    });
}

//--------------------------------------------------------------------------------------------------
StatusDialog::~StatusDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void StatusDialog::addMessage(const QString& message)
{
    ui->edit_status->appendPlainText(
        QTime::currentTime().toString() + QLatin1Char(' ') + message);
}

//--------------------------------------------------------------------------------------------------
void StatusDialog::addMessageAndActivate(const QString& message)
{
    if (!isVisible())
    {
        LOG(INFO) << "Window is hidden. Show and activate";
        show();
        activateWindow();
    }

    addMessage(message);
}

//--------------------------------------------------------------------------------------------------
void StatusDialog::retranslateUi()
{
    ui->retranslateUi(this);
}

//--------------------------------------------------------------------------------------------------
void StatusDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";
    QDialog::closeEvent(event);
}

} // namespace common
