//
// PROJECT:         Aspia
// FILE:            client/ui/file_progress_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_progress_dialog.h"

namespace aspia {

FileProgressDialog::FileProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
}

FileProgressDialog::~FileProgressDialog() = default;

} // namespace aspia
