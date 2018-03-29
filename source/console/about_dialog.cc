//
// PROJECT:         Aspia
// FILE:            console/about_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/about_dialog.h"

#include <QAbstractButton>

namespace aspia {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked, [this](QAbstractButton* /* button */)
    {
        close();
    });
}

AboutDialog::~AboutDialog() = default;

} // namespace aspia
