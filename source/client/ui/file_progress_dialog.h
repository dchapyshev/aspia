//
// PROJECT:         Aspia
// FILE:            client/ui/file_progress_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H
#define _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H

#include <QDialog>

#include "qt/ui_file_progress_dialog.h"

namespace aspia {

class FileProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileProgressDialog(QWidget* parent);
    ~FileProgressDialog();

private:
    Ui::FileProgressDialog ui;

    Q_DISABLE_COPY(FileProgressDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_PROGRESS_DIALOG_H
