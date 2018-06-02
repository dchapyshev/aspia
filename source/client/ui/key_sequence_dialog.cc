//
// PROJECT:         Aspia
// FILE:            client/ui/key_sequence_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/key_sequence_dialog.h"

#include <QAbstractButton>

namespace aspia {

KeySequenceDialog::KeySequenceDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFixedSize(size());

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &KeySequenceDialog::onButtonBoxClicked);
}

KeySequenceDialog::~KeySequenceDialog() = default;

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
