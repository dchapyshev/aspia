//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UI_UPDATE_DIALOG_H
#define COMMON_UI_UPDATE_DIALOG_H

#include <QDialog>

#include "common/update_checker.h"
#include "common/update_info.h"

namespace Ui {
class UpdateDialog;
} // namespace Ui

namespace common {

class UpdateDialog final : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(const QString& update_server,
                 const QString& package_name,
                 QWidget* parent = nullptr);
    UpdateDialog(const UpdateInfo& update_info, QWidget* parent = nullptr);
    ~UpdateDialog() final;

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onUpdateNow();
    void onUpdateCheckedFinished(const QByteArray& result);

private:
    void initialize();

    std::unique_ptr<Ui::UpdateDialog> ui;
    UpdateInfo update_info_;

    std::unique_ptr<UpdateChecker> checker_;
    bool checker_finished_ = true;

    Q_DISABLE_COPY(UpdateDialog)
};

} // namespace common

#endif // COMMON_UI_UPDATE_DIALOG_H
