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

#include "client/ui/desktop/record_settings_dialog.h"

#include "base/logging.h"
#include "client/ui/desktop/desktop_settings.h"

#include <QFileDialog>
#include <QTimer>

namespace client {

//--------------------------------------------------------------------------------------------------
RecordSettingsDialog::RecordSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    DesktopSettings settings;
    ui.checkbox_autostart->setChecked(settings.recordSessions());
    ui.edit_dir->setText(settings.recordingPath());

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &RecordSettingsDialog::onButtonBoxClicked);

    connect(ui.button_select_dir, &QPushButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] Select directory";

        QString path = QFileDialog::getExistingDirectory(
            this, tr("Choose path"), ui.edit_dir->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (path.isEmpty())
        {
            LOG(INFO) << "[ACTION] Directory selection rejected";
            return;
        }

        LOG(INFO) << "[ACTION] Directory selected:" << path;
        ui.edit_dir->setText(path);
    });

    QTimer::singleShot(0, this, [this]()
    {
        setFixedHeight(sizeHint().height());
        adjustSize();
    });
}

//--------------------------------------------------------------------------------------------------
RecordSettingsDialog::~RecordSettingsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RecordSettingsDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";
        reject();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";

        DesktopSettings settings;
        settings.setRecordSessions(ui.checkbox_autostart->isChecked());
        settings.setRecordingPath(ui.edit_dir->text());

        accept();
    }

    close();
}

} // namespace client
