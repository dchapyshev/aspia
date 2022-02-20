//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__UI__UPDATE_DIALOG_H
#define COMMON__UI__UPDATE_DIALOG_H

#include "base/macros_magic.h"
#include "common/ui/update_info.h"

#include <QDialog>
#include <QPointer>

namespace Ui {
class UpdateDialog;
} // namespace Ui

namespace common {

class UpdateChecker;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(const QString& update_server,
                 const QString& package_name,
                 QWidget* parent = nullptr);
    UpdateDialog(const UpdateInfo& update_info, QWidget* parent = nullptr);
    ~UpdateDialog() override;

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onUpdateChecked(const QByteArray& result);
    void onUpdateNow();

private:
    void initialize();

    std::unique_ptr<Ui::UpdateDialog> ui;
    UpdateInfo update_info_;

    QPointer<UpdateChecker> checker_;
    bool checker_finished_ = true;

    DISALLOW_COPY_AND_ASSIGN(UpdateDialog);
};

} // namespace common

#endif // COMMON__UI__UPDATE_DIALOG_H
