//
// PROJECT:         Aspia
// FILE:            client/ui/key_sequence_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H
#define _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H

#include "base/macros.h"
#include "qt/ui_key_sequence_dialog.h"

namespace aspia {

class KeySequenceDialog : public QDialog
{
    Q_OBJECT

public:
    ~KeySequenceDialog() = default;

    static QKeySequence keySequence(QWidget* parent = nullptr);

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    KeySequenceDialog(QWidget* parent);

    Ui::KeySequenceDialog ui;

    DISALLOW_COPY_AND_ASSIGN(KeySequenceDialog);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__KEY_SEQUENCE_DIALOG_H
