//
// PROJECT:         Aspia
// FILE:            client/ui/key_sequence_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H
#define _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H

#include "ui_key_sequence_dialog.h"

namespace aspia {

class KeySequenceDialog : public QDialog
{
    Q_OBJECT

public:
    ~KeySequenceDialog();

    static QKeySequence keySequence(QWidget* parent = nullptr);

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    explicit KeySequenceDialog(QWidget* parent);

    Ui::KeySequenceDialog ui;

    Q_DISABLE_COPY(KeySequenceDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H
