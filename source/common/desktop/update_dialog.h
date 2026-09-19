//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_DESKTOP_UPDATE_DIALOG_H
#define COMMON_DESKTOP_UPDATE_DIALOG_H

#include <QDialog>

#include <memory>

#include "base/scoped_qpointer.h"
#include "common/update_info.h"

namespace Ui {
class UpdateDialog;
} // namespace Ui

class ElevateUtil;
class UpdateChecker;
class UpdateInstaller;

class UpdateDialog final : public QDialog
{
    Q_OBJECT

public:
    // What to do with an update once the check has found one.
    enum class Action
    {
        ASK,    // Show it and wait for the user to decide.
        INSTALL // Install it. The user has decided already, in the process that started this one.
    };

    // What the process started to install an update returns to the one that started it. Any other
    // code means it never got as far as its own window and has reported nothing to the user.
    static constexpr int kInstalledExitCode = 0;
    static constexpr int kClosedExitCode = 1;

    UpdateDialog(const QString& channel, const QString& package, Action action,
                 QWidget* parent = nullptr);
    ~UpdateDialog() final;

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onUpdateNow();
    void onUpdateCheckFinished(const UpdateInfo& update_info);
    void onUpdateCheckFailed();

private:
    void startInstall();
    void startPrivilegedInstance();
    void setInstalling(bool installing);
    void destroyChecker();

    std::unique_ptr<Ui::UpdateDialog> ui;
    const QString channel_;
    const Action action_;
    UpdateInfo update_info_;

    std::unique_ptr<UpdateChecker> checker_;

    ScopedQPointer<UpdateInstaller> installer_;
    ScopedQPointer<ElevateUtil> elevate_util_;

    Q_DISABLE_COPY_MOVE(UpdateDialog)
};

#endif // COMMON_DESKTOP_UPDATE_DIALOG_H
