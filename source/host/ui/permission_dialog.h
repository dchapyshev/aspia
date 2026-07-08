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

#ifndef HOST_UI_PERMISSION_DIALOG_H
#define HOST_UI_PERMISSION_DIALOG_H

#include <QDialog>
#include <QList>

#include "base/mac/permissions.h"

class QGridLayout;
class QLabel;
class QPushButton;

// macOS "Review System Access": lists the privacy permissions the host needs, their current state and
// buttons to grant the missing ones. Polls while visible so the state follows changes made in System
// Settings.
class PermissionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PermissionDialog(QWidget* parent = nullptr);
    ~PermissionDialog() final;

    // True if at least one required permission is currently missing.
    static bool hasMissingPermissions();

private:
    void addPermission(MacPermission permission, const QString& title, const QString& description,
                       const QString& action);
    void refresh();

    struct Item
    {
        MacPermission permission;
        QLabel* status = nullptr;
        QPushButton* button = nullptr;
        bool requested = false;  // the one-shot system prompt has already been shown
    };

    QGridLayout* grid_ = nullptr;
    QList<Item> items_;

    Q_DISABLE_COPY_MOVE(PermissionDialog)
};

#endif // HOST_UI_PERMISSION_DIALOG_H
