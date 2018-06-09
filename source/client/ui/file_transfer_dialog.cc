//
// PROJECT:         Aspia
// FILE:            client/ui/file_transfer_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_transfer_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QMessageBox>

namespace aspia {

FileTransferDialog::FileTransferDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setFixedSize(size());

    connect(ui.button_box, &QDialogButtonBox::clicked, [this](QAbstractButton* /* button */)
    {
        close();
    });
}

FileTransferDialog::~FileTransferDialog() = default;

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
