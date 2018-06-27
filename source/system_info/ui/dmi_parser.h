//
// PROJECT:         Aspia
// FILE:            system_info/ui/dmi_parser.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__UI__DMI_PARSER_H
#define _ASPIA_SYSTEM_INFO__UI__DMI_PARSER_H

#include "system_info/ui/parser.h"
#include "ui_dmi_parser.h"

namespace aspia {

class DmiParser : public Parser
{
    Q_OBJECT

public:
    DmiParser(QWidget* parent);

public slots:
    // Parser implementation.
    void readReply(const QString& uuid, const QByteArray& data) override;

private:
    Ui::DmiParser ui;

    Q_DISABLE_COPY(DmiParser)
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__UI__DMI_PARSER_H
