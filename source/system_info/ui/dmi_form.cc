//
// PROJECT:         Aspia
// FILE:            system_info/ui/dmi_form.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/ui/dmi_form.h"

namespace aspia {

DmiForm::DmiForm(QWidget* parent, const QString& uuid)
    : Form(parent, uuid)
{
    ui.setupUi(this);
}

// static
Form* DmiForm::create(QWidget* parent, const QString& uuid)
{
    return new DmiForm(parent, uuid);
}

void DmiForm::readReply(const QString& uuid, const QByteArray& data)
{

}

} // namespace aspia
