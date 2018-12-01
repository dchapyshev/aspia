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

#include "client/ui/file_remove_dialog.h"

#include <QPushButton>
#include <QMessageBox>

#if defined(OS_WIN)
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif

namespace aspia {

FileRemoveDialog::FileRemoveDialog(QWidget* parent)
    : QDialog(parent)
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
}

FileRemoveDialog::~FileRemoveDialog()
{
#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->hide();
#endif
}

void FileRemoveDialog::setProgress(const QString& current_item, int percentage)
{
    QFontMetrics metrics(ui.label_current_item->font());

    QString elided_text = metrics.elidedText(tr("Deleting: %1").arg(current_item),
                                             Qt::ElideMiddle,
                                             ui.label_current_item->width());

    ui.label_current_item->setText(elided_text);
    ui.progress->setValue(percentage);

#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->setValue(percentage);
#endif
}

void FileRemoveDialog::showError(FileRemover* remover,
                                 FileRemover::Actions actions,
                                 const QString& message)
{
#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->pause();
#endif

    QPointer<QMessageBox> dialog(new QMessageBox(this));

    dialog->setWindowTitle(tr("Warning"));
    dialog->setIcon(QMessageBox::Warning);
    dialog->setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;
    QAbstractButton* abort_button = nullptr;

    if (actions & FileRemover::Skip)
        skip_button = dialog->addButton(tr("Skip"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::SkipAll)
        skip_all_button = dialog->addButton(tr("Skip All"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::Abort)
        abort_button = dialog->addButton(tr("Abort"), QMessageBox::ButtonRole::ActionRole);

    connect(dialog, &QMessageBox::buttonClicked, [&](QAbstractButton* button)
    {
        if (button != nullptr)
        {
            if (button == skip_button)
            {
                remover->applyAction(FileRemover::Skip);
                return;
            }
            else if (button == skip_all_button)
            {
                remover->applyAction(FileRemover::SkipAll);
                return;
            }
        }

        remover->applyAction(FileRemover::Abort);
    });

    dialog->exec();

#if defined(OS_WIN)
    if (taskbar_progress_)
        taskbar_progress_->resume();
#endif
}

} // namespace aspia
