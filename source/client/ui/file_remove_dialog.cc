//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_remove_dialog.h"

#include "base/logging.h"
#include "client/file_remove_window_proxy.h"
#include "client/file_remover_proxy.h"
#include "client/ui/file_error_code.h"
#include "qt_base/application.h"

#include <QCloseEvent>
#include <QPointer>
#include <QPushButton>
#include <QMessageBox>

#if defined(OS_WIN)
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif // defined(OS_WIN)

namespace client {

FileRemoveDialog::FileRemoveDialog(QWidget* parent)
    : QDialog(parent),
      remover_window_proxy_(std::make_shared<FileRemoveWindowProxy>(
          qt_base::Application::taskRunner(), this))
{
    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &FileRemoveDialog::close);

#if defined(OS_WIN)
    QWinTaskbarButton* button = new QWinTaskbarButton(this);

    button->setWindow(parent->windowHandle());

    taskbar_progress_ = button->progress();
    if (taskbar_progress_)
        taskbar_progress_->show();
#endif

    label_metrics_ = std::make_unique<QFontMetrics>(ui.label_current_item->font());
}

FileRemoveDialog::~FileRemoveDialog()
{
    remover_window_proxy_->dettach();

#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif
}

void FileRemoveDialog::start(std::shared_ptr<FileRemoverProxy> remover_proxy)
{
    remover_proxy_ = std::move(remover_proxy);
    DCHECK(remover_proxy_);

    show();
    activateWindow();
}

void FileRemoveDialog::stop()
{
    stopped_ = true;
    close();
}

void FileRemoveDialog::setCurrentProgress(const std::string& name, int percentage)
{
    QString elided_text = label_metrics_->elidedText(
        tr("Deleting: %1").arg(QString::fromStdString(name)),
        Qt::ElideMiddle,
        ui.label_current_item->width());

    ui.label_current_item->setText(elided_text);
    ui.progress->setValue(percentage);

#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->setValue(percentage);
#endif
}

void FileRemoveDialog::errorOccurred(const std::string& path,
                                     proto::FileError error_code,
                                     uint32_t available_actions)
{
#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->pause();
#endif

    QString message;

    if (path.empty())
    {
        message = tr("An error occurred while retrieving the list of files: %1")
            .arg(fileErrorToString(error_code));
    }
    else
    {
        message = tr("Failed to delete \"%1\": %2.")
            .arg(QString::fromStdString(path))
            .arg(fileErrorToString(error_code));
    }

    QPointer<QMessageBox> dialog(new QMessageBox(this));

    dialog->setWindowTitle(tr("Warning"));
    dialog->setIcon(QMessageBox::Warning);
    dialog->setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;

    if (available_actions & FileRemover::ACTION_SKIP)
        skip_button = dialog->addButton(tr("Skip"), QMessageBox::ButtonRole::ActionRole);

    if (available_actions & FileRemover::ACTION_SKIP_ALL)
        skip_all_button = dialog->addButton(tr("Skip All"), QMessageBox::ButtonRole::ActionRole);

    if (available_actions & FileRemover::ACTION_ABORT)
        dialog->addButton(tr("Abort"), QMessageBox::ButtonRole::ActionRole);

    connect(dialog, &QMessageBox::buttonClicked, [&](QAbstractButton* button)
    {
        if (button != nullptr)
        {
            if (button == skip_button)
            {
                remover_proxy_->setAction(FileRemover::ACTION_SKIP);
                return;
            }

            if (button == skip_all_button)
            {
                remover_proxy_->setAction(FileRemover::ACTION_SKIP_ALL);
                return;
            }
        }

        remover_proxy_->setAction(FileRemover::ACTION_ABORT);
    });

    dialog->exec();

#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->resume();
#endif
}

void FileRemoveDialog::closeEvent(QCloseEvent* event)
{
    if (stopped_)
    {
        event->accept();
        accept();
    }
    else
    {
        remover_proxy_->stop();
        event->ignore();
    }
}

} // namespace client
