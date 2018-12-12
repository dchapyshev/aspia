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

#ifndef ASPIA_UPDATER__UPDATE_DIALOG_H_
#define ASPIA_UPDATER__UPDATE_DIALOG_H_

#include <QDialog>

#include "base/macros_magic.h"
#include "updater/update_info.h"

namespace Ui {
class UpdateDialog;
} // namespace Ui

namespace aspia {

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(const QString& update_server,
                 const QString& package_name,
                 QWidget* parent = nullptr);
    UpdateDialog(const UpdateInfo& update_info, QWidget* parent = nullptr);
    ~UpdateDialog();

private slots:
    void onUpdateNow();

private:
    void initialize();

    std::unique_ptr<Ui::UpdateDialog> ui;
    UpdateInfo update_info_;

    DISALLOW_COPY_AND_ASSIGN(UpdateDialog);
};

} // namespace aspia

#endif // ASPIA_UPDATER__UPDATE_DIALOG_H_
