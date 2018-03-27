//
// PROJECT:         Aspia
// FILE:            client/ui/file_remove_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_remove_dialog.h"

#include <QPushButton>
#include <QMessageBox>

namespace aspia {

FileRemoveDialog::FileRemoveDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setFixedSize(size());

    connect(ui.button_box, SIGNAL(clicked(QAbstractButton*)), SLOT(close()));
}

FileRemoveDialog::~FileRemoveDialog() = default;

void FileRemoveDialog::setProgress(const QString& current_item, int percentage)
{
    QFontMetrics metrics(ui.label_current_item->font());

    QString elided_text = metrics.elidedText(tr("Deleting: %1").arg(current_item),
                                             Qt::ElideMiddle,
                                             ui.label_current_item->width());

    ui.label_current_item->setText(elided_text);
    ui.progress->setValue(percentage);
}

void FileRemoveDialog::showError(FileRemover* remover,
                                 FileRemover::Actions actions,
                                 const QString& message)
{
    QMessageBox dialog(this);

    dialog.setWindowTitle(tr("Warning"));
    dialog.setIcon(QMessageBox::Warning);
    dialog.setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;
    QAbstractButton* abort_button = nullptr;

    if (actions & FileRemover::Skip)
        skip_button = dialog.addButton(tr("Skip"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::SkipAll)
        skip_all_button = dialog.addButton(tr("Skip All"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::Abort)
        abort_button = dialog.addButton(tr("Abort"), QMessageBox::ButtonRole::ActionRole);

    dialog.exec();

    QAbstractButton* button = dialog.clickedButton();
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
}

} // namespace aspia
