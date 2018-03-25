//
// PROJECT:         Aspia
// FILE:            client/ui/file_progress_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H
#define _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H

#include <QDialog>

#include "base/macros.h"
#include "qt/ui_file_progress_dialog.h"

namespace aspia {

class FileProgressDialog : public QDialog
{
    Q_OBJECT

public:
    FileProgressDialog(QWidget* parent);
    ~FileProgressDialog();

private:
    Ui::FileProgressDialog ui;

    DISALLOW_COPY_AND_ASSIGN(FileProgressDialog);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H
