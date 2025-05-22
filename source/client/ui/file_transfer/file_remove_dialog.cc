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

#include "client/ui/file_transfer/file_remove_dialog.h"

#include "base/logging.h"
#include "client/ui/file_transfer/file_error_code.h"

#include <QCloseEvent>
#include <QPointer>
#include <QPushButton>
#include <QMessageBox>

// Removed completely in qt6.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif // defined(Q_OS_WINDOWS)
#endif

namespace client {

//--------------------------------------------------------------------------------------------------
FileRemoveDialog::FileRemoveDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(LS_INFO) << "Ctor";

    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &FileRemoveDialog::close);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    QWinTaskbarButton* button = new QWinTaskbarButton(this);

    button->setWindow(parent->windowHandle());

    taskbar_progress_ = button->progress();
    if (taskbar_progress_)
        taskbar_progress_->show();
#endif
#endif

    label_metrics_ = std::make_unique<QFontMetrics>(ui.label_current_item->font());
}

//--------------------------------------------------------------------------------------------------
FileRemoveDialog::~FileRemoveDialog()
{
    LOG(LS_INFO) << "Dtor";

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveDialog::start()
{
    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveDialog::stop()
{
    if (stopped_)
        return;

    stopped_ = true;
    close();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveDialog::setCurrentProgress(const QString& name, int percentage)
{
    QString elided_text = label_metrics_->elidedText(
        tr("Deleting: %1").arg(name),
        Qt::ElideMiddle,
        ui.label_current_item->width());

    ui.label_current_item->setText(elided_text);
    ui.progress->setValue(percentage);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->setValue(percentage);
#endif
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveDialog::errorOccurred(const QString& path,
                                     proto::FileError error_code,
                                     quint32 available_actions)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->pause();
#endif
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

    connect(dialog, &QMessageBox::buttonClicked, this, [&](QAbstractButton* button)
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

    dialog->exec();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    if (taskbar_progress_)
        taskbar_progress_->resume();
#endif
#endif
}

//--------------------------------------------------------------------------------------------------
void FileRemoveDialog::closeEvent(QCloseEvent* event)
{
    if (stopped_)
    {
        event->accept();
        accept();
    }
    else
    {
        emit sig_stop();
        event->ignore();
    }
}

} // namespace client
