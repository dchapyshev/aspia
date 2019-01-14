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

#ifndef HOST__UI__HOST_CONFIG_DIALOG_H
#define HOST__UI__HOST_CONFIG_DIALOG_H

#include "common/locale_loader.h"
#include "net/srp_user.h"
#include "ui_host_config_dialog.h"

namespace host {

class HostConfigDialog : public QDialog
{
    Q_OBJECT

public:
    HostConfigDialog(common::LocaleLoader& locale_loader, QWidget* parent = nullptr);
    ~HostConfigDialog() = default;

    static bool importSettings(const QString& path, bool silent, QWidget* parent = nullptr);
    static bool exportSettings(const QString& path, bool silent, QWidget* parent = nullptr);

private slots:
    void onUserContextMenu(const QPoint& point);
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onServiceInstallRemove();
    void onServiceStartStop();
    void onImport();
    void onExport();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void createLanguageList(const QString& current_locale);
    void retranslateUi(const QString& locale);
    void setConfigChanged(bool changed);
    bool isConfigChanged() const;
    void reloadAll();
    void reloadUserList();
    void reloadServiceStatus();
    bool isServiceStarted();
    bool installService();
    bool removeService();
    bool startService();
    bool stopService();
    bool restartService();

    Ui::HostConfigDialog ui;

    common::LocaleLoader& locale_loader_;
    net::SrpUserList users_;

    enum class ServiceState { NOT_INSTALLED, ACCESS_DENIED, NOT_STARTED, STARTED };

    ServiceState service_state_;

    DISALLOW_COPY_AND_ASSIGN(HostConfigDialog);
};

} // namespace host

#endif // HOST__UI__HOST_CONFIG_DIALOG_H
