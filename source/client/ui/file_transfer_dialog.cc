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

#include "client/ui/file_transfer_dialog.h"

#include <QPushButton>
#include <QMessageBox>

namespace aspia {

FileTransferDialog::FileTransferDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    connect(ui.button_box, &QDialogButtonBox::clicked, [this](QAbstractButton* /* button */)
    {
        close();
    });
}

void FileTransferDialog::setCurrentItem(const QString& source_path, const QString& target_path)
{
    if (task_queue_building_)
    {
        task_queue_building_ = false;
        ui.label_task->setText(tr("Current Task: Copying items."));
    }

    QFontMetrics source_label_metrics(ui.label_source->font());
    QFontMetrics target_label_metrics(ui.label_target->font());

    QString source_text = source_label_metrics.elidedText(
        tr("From: %1").arg(source_path),
        Qt::ElideMiddle,
        ui.label_source->width());

    QString target_text = target_label_metrics.elidedText(
        tr("To: %1").arg(target_path),
        Qt::ElideMiddle,
        ui.label_target->width());

    ui.label_source->setText(source_text);
    ui.label_target->setText(target_text);
}

void FileTransferDialog::setProgress(int total, int current)
{
    ui.progress_total->setValue(total);
    ui.progress_current->setValue(current);
}

void FileTransferDialog::showError(FileTransfer* transfer,
                                   FileTransfer::Error error_type,
                                   const QString& message)
{
    QMessageBox dialog(this);

    dialog.setWindowTitle(tr("Warning"));
    dialog.setIcon(QMessageBox::Warning);
    dialog.setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;
    QAbstractButton* replace_button = nullptr;
    QAbstractButton* replace_all_button = nullptr;
    QAbstractButton* abort_button = nullptr;

    FileTransfer::Actions actions = transfer->availableActions(error_type);

    if (actions & FileTransfer::Skip)
        skip_button = dialog.addButton(tr("Skip"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileTransfer::SkipAll)
        skip_all_button = dialog.addButton(tr("Skip All"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileTransfer::Replace)
        replace_button = dialog.addButton(tr("Replace"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileTransfer::ReplaceAll)
        replace_all_button = dialog.addButton(tr("Replace All"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileTransfer::Abort)
        abort_button = dialog.addButton(tr("Abort"), QMessageBox::ButtonRole::ActionRole);

    dialog.exec();

    QAbstractButton* button = dialog.clickedButton();
    if (button != nullptr)
    {
        if (button == skip_button)
        {
            transfer->applyAction(error_type, FileTransfer::Skip);
            return;
        }
        else if (button == skip_all_button)
        {
            transfer->applyAction(error_type, FileTransfer::SkipAll);
            return;
        }
        else if (button == replace_button)
        {
            transfer->applyAction(error_type, FileTransfer::Replace);
            return;
        }
        else if (button == replace_all_button)
        {
            transfer->applyAction(error_type, FileTransfer::ReplaceAll);
            return;
        }
    }

    transfer->applyAction(error_type, FileTransfer::Abort);
}

} // namespace aspia
