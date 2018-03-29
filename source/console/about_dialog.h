//
// PROJECT:         Aspia
// FILE:            console/about_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ABOUT_DIALOG_H
#define _ASPIA_CONSOLE__ABOUT_DIALOG_H

#include <QDialog>

#include "qt/ui_about_dialog.h"

namespace aspia {

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

private:
    Ui::AboutDialog ui;

    Q_DISABLE_COPY(AboutDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ABOUT_DIALOG_H
