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

#ifndef ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H_
#define ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H_

#include "base/locale_loader.h"
#include "host/user.h"
#include "ui_host_config_dialog.h"

namespace aspia {

class HostConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HostConfigDialog(QWidget* parent = nullptr);
    ~HostConfigDialog() = default;

private slots:
    void onUserContextMenu(const QPoint& point);
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void createLanguageList(const QString& current_locale);
    void retranslateUi(const QString& locale);
    void setConfigChanged(bool changed);
    bool isConfigChanged() const;
    void reloadUserList();
    bool isServiceStarted();
    bool restartService();

    Ui::HostConfigDialog ui;

    LocaleLoader locale_loader_;
    QList<User> user_list_;

    DISALLOW_COPY_AND_ASSIGN(HostConfigDialog);
};

} // namespace aspia

#endif // ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H_
