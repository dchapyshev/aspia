//
// PROJECT:         Aspia
// FILE:            client/ui/file_remove_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_remove_dialog.h"

namespace aspia {

FileRemoveDialog::FileRemoveDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_box, SIGNAL(clicked(QAbstractButton*)), SLOT(close()));
}

FileRemoveDialog::~FileRemoveDialog() = default;

void FileRemoveDialog::setProgress(const QString& current_item, int percentage)
{
    ui.label_current_item->setText(current_item);
    ui.progress->setValue(percentage);
}

void FileRemoveDialog::closeEvent(QCloseEvent* event)
{
    emit cancel();
    QDialog::closeEvent(event);
}

} // namespace aspia
