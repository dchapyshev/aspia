//
// PROJECT:         Aspia
// FILE:            client/ui/file_remove_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H
#define _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H

#include <QDialog>

#include "base/macros.h"
#include "qt/ui_file_remove_dialog.h"

namespace aspia {

class FileRemoveDialog : public QDialog
{
    Q_OBJECT

public:
    FileRemoveDialog(QWidget* parent);
    ~FileRemoveDialog();

signals:
    void cancel();

public slots:
    void setProgress(const QString& current_item, int percentage);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    Ui::FileRemoveDialog ui;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveDialog);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H
