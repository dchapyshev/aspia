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

#include "client/ui/file_transfer/file_transfer_widget.h"

#include "base/logging.h"
#include "client/ui/file_transfer/file_error_code.h"
#include "common/ui/formatter.h"
#include "common/ui/msg_box.h"
#include "ui_file_transfer_widget.h"

#if defined(Q_OS_WINDOWS)
#include "common/ui/taskbar_button.h"
#include "common/ui/taskbar_progress.h"
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
FileTransferWidget::FileTransferWidget(QWidget* parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::FileTransferWidget>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->progress_total->setRange(0, 0);
    ui->progress_current->setRange(0, 0);

    connect(ui->button_cancel, &QPushButton::clicked, this, &FileTransferWidget::requestStop);

#if defined(Q_OS_WINDOWS)
    TaskbarButton* button = new TaskbarButton(this);
    taskbar_progress_ = button->progress();
    if (taskbar_progress_)
        taskbar_progress_->setRange(0, 0);
#endif

    label_metrics_ = std::make_unique<QFontMetrics>(ui->label_source->font());
}

//--------------------------------------------------------------------------------------------------
FileTransferWidget::~FileTransferWidget()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::reset()
{
    task_queue_building_ = true;
    stopping_ = false;
    finished_ = false;

    ui->label_task->setText(tr("Creating a list of files to copy..."));
    ui->label_source->setText("...");
    ui->label_target->setText("...");
    ui->label_speed->setText("...");

    ui->progress_total->setRange(0, 0);
    ui->progress_total->setValue(0);
    ui->progress_current->setRange(0, 0);
    ui->progress_current->setValue(0);

    ui->button_cancel->setEnabled(true);

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
    {
        taskbar_progress_->resume();
        taskbar_progress_->setRange(0, 0);
        taskbar_progress_->setValue(0);
    }
#endif
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::requestStop()
{
    if (finished_ || stopping_)
        return;

    stopping_ = true;

    ui->label_task->setText(tr("Cancel transfer of files."));
    ui->button_cancel->setEnabled(false);

    emit sig_stop();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::start()
{
    updateTaskbarWindow();

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->show();
#endif
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::stop()
{
    finished_ = true;

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif

    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::setCurrentItem(const QString& source_path, const QString& target_path)
{
    if (task_queue_building_)
    {
        task_queue_building_ = false;
        ui->label_task->setText(tr("Copying items."));

        ui->progress_total->setRange(0, 100);
        ui->progress_current->setRange(0, 100);

#if defined(Q_OS_WINDOWS)
        if (taskbar_progress_)
            taskbar_progress_->setRange(0, 100);
#endif
    }

    ui->label_source->setText(label_metrics_->elidedText(
        source_path, Qt::ElideMiddle, ui->label_source->width()));
    ui->label_target->setText(label_metrics_->elidedText(
        target_path, Qt::ElideMiddle, ui->label_target->width()));
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::setCurrentProgress(int total, int current)
{
    ui->progress_total->setValue(total);
    ui->progress_current->setValue(current);

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->setValue(total);
#endif
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::setCurrentSpeed(qint64 speed)
{
    ui->label_speed->setText(Formatter::transferSpeedToString(speed));
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::errorOccurred(const FileTransfer::Error& error)
{
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->pause();
#endif

    MsgBox* dialog = new MsgBox(window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);

    dialog->setWindowTitle(tr("Warning"));
    dialog->setIcon(MsgBox::Warning);
    dialog->setText(errorToMessage(error));

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;
    QAbstractButton* replace_button = nullptr;
    QAbstractButton* replace_all_button = nullptr;

    const quint32 available_actions = error.availableActions();

    if (available_actions & FileTransfer::Error::ACTION_SKIP)
        skip_button = dialog->addButton(tr("Skip"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileTransfer::Error::ACTION_SKIP_ALL)
        skip_all_button = dialog->addButton(tr("Skip All"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileTransfer::Error::ACTION_REPLACE)
        replace_button = dialog->addButton(tr("Replace"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileTransfer::Error::ACTION_REPLACE_ALL)
        replace_all_button = dialog->addButton(tr("Replace All"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileTransfer::Error::ACTION_ABORT)
        dialog->addButton(tr("Abort"), MsgBox::ButtonRole::ActionRole);

    FileTransfer::Error::Type error_type = error.type();

    connect(dialog, &MsgBox::buttonClicked, this,
            [this, error_type, skip_button, skip_all_button, replace_button, replace_all_button]
            (QAbstractButton* button)
    {
        if (button != nullptr)
        {
            if (button == skip_button)
            {
                emit sig_action(error_type, FileTransfer::Error::ACTION_SKIP);
                return;
            }

            if (button == skip_all_button)
            {
                emit sig_action(error_type, FileTransfer::Error::ACTION_SKIP_ALL);
                return;
            }

            if (button == replace_button)
            {
                emit sig_action(error_type, FileTransfer::Error::ACTION_REPLACE);
                return;
            }

            if (button == replace_all_button)
            {
                emit sig_action(error_type, FileTransfer::Error::ACTION_REPLACE_ALL);
                return;
            }
        }

        emit sig_action(error_type, FileTransfer::Error::ACTION_ABORT);
    });

    connect(dialog, &MsgBox::finished, this, [this]()
    {
#if defined(Q_OS_WINDOWS)
        if (taskbar_progress_)
            taskbar_progress_->resume();
#endif
    });

    dialog->show();
}

//--------------------------------------------------------------------------------------------------
QString FileTransferWidget::errorToMessage(const FileTransfer::Error& error)
{
    switch (error.type())
    {
        case FileTransfer::Error::Type::QUEUE:
        {
            return tr("An error occurred while building the file queue for copying");
        }

        case FileTransfer::Error::Type::CREATE_DIRECTORY:
        {
            return tr("Failed to create directory \"%1\": %2")
                .arg(error.path(), fileErrorToString(error.code()));
        }

        case FileTransfer::Error::Type::CREATE_FILE:
        case FileTransfer::Error::Type::ALREADY_EXISTS:
        {
            return tr("Failed to create file \"%1\": %2")
                .arg(error.path(), fileErrorToString(error.code()));
        }

        case FileTransfer::Error::Type::OPEN_FILE:
        {
            return tr("Failed to open file \"%1\": %2")
                .arg(error.path(), fileErrorToString(error.code()));
        }

        case FileTransfer::Error::Type::WRITE_FILE:
        {
            return tr("Failed to write file \"%1\": %2")
                .arg(error.path(), fileErrorToString(error.code()));
        }

        case FileTransfer::Error::Type::READ_FILE:
        {
            return tr("Failed to read file \"%1\": %2")
                .arg(error.path(), fileErrorToString(error.code()));
        }

        default:
        {
            return tr("Unknown error type while copying files");
        }
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWidget::updateTaskbarWindow()
{
#if defined(Q_OS_WINDOWS)
    if (!taskbar_progress_)
        return;

    QWidget* top_window = window();
    if (!top_window)
        return;

    TaskbarButton* button = qobject_cast<TaskbarButton*>(taskbar_progress_->parent());
    if (button)
        button->setWindow(top_window->windowHandle());
#endif
}
