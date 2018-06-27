//
// PROJECT:         Aspia
// FILE:            system_info/ui/dmi_parser.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/ui/dmi_parser.h"

namespace aspia {

DmiParser::DmiParser(QWidget* parent)
    : Parser(parent)
{
    ui.setupUi(this);
}

void DmiParser::readReply(const QString& uuid, const QByteArray& data)
{

}

} // namespace aspia
