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

#ifndef HOST_UI_CONFIG_DIALOG_H
#define HOST_UI_CONFIG_DIALOG_H

#include <QDialog>

#include <memory>

#include "base/location.h"

class QAbstractButton;
class QTreeWidgetItem;

namespace Ui {
class ConfigDialog;
} // namespace Ui

class UserList;

class ConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget* parent = nullptr);
    ~ConfigDialog() final;

private slots:
    void onOneTimeStateChanged(int state);
    void onConnConfirmStateChanged(int state);
    void onUserContextMenu(const QPoint& point);
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onPassProtectClicked();
    void onChangePassClicked();
    void onImport();
    void onExport();
    void onConfigChanged() { setConfigChanged(FROM_HERE, true); }
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void setConfigChanged(const Location& location, bool changed);
    bool isConfigChanged() const;
    void reloadAll();
    void reloadUserList(const UserList& user_list);

    std::unique_ptr<Ui::ConfigDialog> ui;
    Q_DISABLE_COPY_MOVE(ConfigDialog)
};

#endif // HOST_UI_CONFIG_DIALOG_H
