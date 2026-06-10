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

#include "client/desktop/file_transfer/file_remove_widget.h"

#include <QPointer>

#include "base/logging.h"
#include "client/desktop/file_transfer/file_error_code.h"
#include "common/desktop/msg_box.h"
#include "ui_file_remove_widget.h"

#if defined(Q_OS_WINDOWS)
#include "common/desktop/taskbar_button.h"
#include "common/desktop/taskbar_progress.h"
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
FileRemoveWidget::FileRemoveWidget(QWidget* parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::FileRemoveWidget>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    connect(ui->button_cancel, &QPushButton::clicked, this, &FileRemoveWidget::requestStop);

#if defined(Q_OS_WINDOWS)
    TaskbarButton* button = new TaskbarButton(this);
    taskbar_progress_ = button->progress();
#endif

    label_metrics_ = std::make_unique<QFontMetrics>(ui->label_current_item->font());
}

//--------------------------------------------------------------------------------------------------
FileRemoveWidget::~FileRemoveWidget()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::reset()
{
    queue_building_ = true;
    stopping_ = false;
    finished_ = false;

    ui->label_task->setText(tr("Creating a list of files to delete..."));
    ui->label_current_item->setText("...");
    ui->progress->setValue(0);

    ui->button_cancel->setEnabled(true);

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
    {
        taskbar_progress_->resume();
        taskbar_progress_->setValue(0);
    }
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::requestStop()
{
    if (finished_ || stopping_)
        return;

    stopping_ = true;

    ui->label_task->setText(tr("Cancel removal of files."));
    ui->button_cancel->setEnabled(false);

    emit sig_stop();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::start()
{
    updateTaskbarWindow();

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->show();
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::stop()
{
    if (finished_)
        return;

    finished_ = true;

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif

    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::setCurrentProgress(const QString& name, int percentage)
{
    if (queue_building_)
    {
        queue_building_ = false;
        ui->label_task->setText(tr("Deleting items."));
    }

    ui->label_current_item->setText(label_metrics_->elidedText(
        name, Qt::ElideMiddle, ui->label_current_item->width()));
    ui->progress->setValue(percentage);

#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->setValue(percentage);
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveWidget::errorOccurred(const QString& path,
                                     proto::file_transfer::ErrorCode error_code,
                                     quint32 available_actions)
{
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->pause();
#endif

    QString message;

    if (path.isEmpty())
    {
        message = tr("An error occurred while retrieving the list of files: %1")
            .arg(fileErrorToString(error_code));
    }
    else
    {
        message = tr("Failed to delete \"%1\": %2.").arg(path, fileErrorToString(error_code));
    }

    MsgBox* dialog = new MsgBox(window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);

    dialog->setWindowTitle(tr("Warning"));
    dialog->setIcon(MsgBox::Warning);
    dialog->setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;

    if (available_actions & FileRemover::ACTION_SKIP)
        skip_button = dialog->addButton(tr("Skip"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileRemover::ACTION_SKIP_ALL)
        skip_all_button = dialog->addButton(tr("Skip All"), MsgBox::ButtonRole::ActionRole);

    if (available_actions & FileRemover::ACTION_ABORT)
        dialog->addButton(tr("Abort"), MsgBox::ButtonRole::ActionRole);

    connect(dialog, &MsgBox::buttonClicked, this,
            [this, skip_button, skip_all_button](QAbstractButton* button)
    {
        if (button != nullptr)
        {
            if (button == skip_button)
            {
                emit sig_action(FileRemover::ACTION_SKIP);
                return;
            }

            if (button == skip_all_button)
            {
                emit sig_action(FileRemover::ACTION_SKIP_ALL);
                return;
            }
        }

        emit sig_action(FileRemover::ACTION_ABORT);
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
void FileRemoveWidget::updateTaskbarWindow()
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
