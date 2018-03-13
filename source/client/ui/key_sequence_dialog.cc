//
// PROJECT:         Aspia
// FILE:            client/ui/key_sequence_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/key_sequence_dialog.h"

namespace aspia {

KeySequenceDialog::KeySequenceDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setFixedSize(size());

    connect(ui.button_box, SIGNAL(clicked(QAbstractButton*)),
            this, SLOT(onButtonBoxClicked(QAbstractButton*)));
}

// static
QKeySequence KeySequenceDialog::keySequence(QWidget* parent)
{
    KeySequenceDialog dialog(parent);
    if (dialog.exec() != KeySequenceDialog::Accepted)
        return QKeySequence();

    return dialog.ui.edit->keySequence();
}

void KeySequenceDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
        accept();
    else
        reject();

    close();
}

} // namespace aspia
