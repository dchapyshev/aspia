//
// PROJECT:         Aspia
// FILE:            system_info/ui/dmi_form.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__UI__DMI_FORM_H
#define _ASPIA_SYSTEM_INFO__UI__DMI_FORM_H

#include "system_info/protocol/dmi.pb.h"
#include "system_info/ui/form.h"
#include "ui_dmi_form.h"

namespace aspia {

class DmiForm : public Form
{
    Q_OBJECT

public:
    static Form* create(QWidget* parent, const QString& uuid);

public slots:
    void readReply(const QString& uuid, const QByteArray& data) override;

protected:
    DmiForm(QWidget* parent, const QString& uuid);

private:
    Ui::DmiForm ui;
    std::unique_ptr<system_info::dmi::Dmi> dmi_;

    Q_DISABLE_COPY(DmiForm)
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__UI__DMI_FORM_H
