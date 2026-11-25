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

#include "client/desktop/desktop/script_output_dialog.h"

#include "base/logging.h"
#include "client/desktop/desktop/ui_script_output_dialog.h"

//--------------------------------------------------------------------------------------------------
ScriptOutputDialog::ScriptOutputDialog(qint32 script_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::ScriptOutputDialog>()),
      script_id_(script_id)
{
    LOG(TRACE) << "Ctor";

    ui->setupUi(this);
    ui->output->startView(kColumns, kRows);
    ui->status->setText(tr("The script is running..."));

#if defined(Q_OS_MACOS)
    setWindowFlag(Qt::Tool);
#endif // defined(Q_OS_MACOS)

    connect(ui->button_box, &QDialogButtonBox::rejected, this, &ScriptOutputDialog::close);
}

//--------------------------------------------------------------------------------------------------
ScriptOutputDialog::~ScriptOutputDialog()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ScriptOutputDialog::appendOutput(const QByteArray& data)
{
    ui->output->writeOutput(data);
}

//--------------------------------------------------------------------------------------------------
void ScriptOutputDialog::setFinished()
{
    finished_ = true;
    ui->status->setText(tr("The script has finished."));
}
