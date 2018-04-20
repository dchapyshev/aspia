//
// PROJECT:         Aspia
// FILE:            client/ui/file_remove_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H
#define _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H

#include "client/file_remover.h"
#include "ui_file_remove_dialog.h"

namespace aspia {

class FileRemoveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileRemoveDialog(QWidget* parent);
    ~FileRemoveDialog();

public slots:
    void setProgress(const QString& current_item, int percentage);
    void showError(FileRemover* remover, FileRemover::Actions actions, const QString& message);

private:
    Ui::FileRemoveDialog ui;

    Q_DISABLE_COPY(FileRemoveDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H
