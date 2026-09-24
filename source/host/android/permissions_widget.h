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

#ifndef HOST_ANDROID_PERMISSIONS_WIDGET_H
#define HOST_ANDROID_PERMISSIONS_WIDGET_H

#include <QList>
#include <QWidget>

class Button;
class Label;

// Takes the place of the whole app while the host lacks a permission. The host does not work without
// any of them, so the user grants all of them in the settings of the system before anything else.
class PermissionsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit PermissionsWidget(QWidget* parent = nullptr);
    ~PermissionsWidget() final;

    // Reads the permissions again and updates the list. Returns true when all of them are granted.
    bool refresh();

private:
    enum class Permission
    {
        ACCESSIBILITY,
        OVERLAY,
        STORAGE,
        NOTIFICATIONS
    };

    struct Row
    {
        Permission permission = Permission::ACCESSIBILITY;
        Button* open_button = nullptr;
        Label* granted_label = nullptr;
    };

    static bool isGranted(Permission permission);
    static void openSettings(Permission permission);

    QList<Row> rows_;

    Q_DISABLE_COPY_MOVE(PermissionsWidget)
};

#endif // HOST_ANDROID_PERMISSIONS_WIDGET_H
