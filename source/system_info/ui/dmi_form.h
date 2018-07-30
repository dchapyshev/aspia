//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_SYSTEM_INFO__UI__DMI_FORM_H_
#define ASPIA_SYSTEM_INFO__UI__DMI_FORM_H_

#include "base/macros_magic.h"
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

    DISALLOW_COPY_AND_ASSIGN(DmiForm);
};

} // namespace aspia

#endif // ASPIA_SYSTEM_INFO__UI__DMI_FORM_H_
