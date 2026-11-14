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

#ifndef CLIENT_STORAGE_INSTANCE_SETTINGS_H
#define CLIENT_STORAGE_INSTANCE_SETTINGS_H

#include <QSettings>

// Stores non-critical UI/preference settings.
//
// On application upgrade (when the stored version is older than the current one), all parameters
// are cleared on first construction in the process. Therefore this class must NOT be used for
// connection-related, security-related or any other critical data: such values should be stored
// in the Database (see client/storage/database.h) or another persistent storage that survives
// upgrades.
class InstanceSettings
{
public:
    InstanceSettings();
    ~InstanceSettings();

    static void select(const QString& id);
    static void remove(const QString& id);

    static QString filePath();
    static void setFilePathForTesting(const QString& file_path);

    QByteArray tabState(const QString& name) const;
    void setTabState(const QString& name, const QByteArray& state);

    bool isLocalGroupExpanded(qint64 group_id) const;
    void setLocalGroupExpanded(qint64 group_id, bool expanded);
    void removeLocalGroupExpanded(qint64 group_id);

    bool isWorkspaceExpanded(qint64 workspace_id) const;
    void setWorkspaceExpanded(qint64 workspace_id, bool expanded);

    bool isRouterGroupExpanded(qint64 group_id) const;
    void setRouterGroupExpanded(qint64 group_id, bool expanded);

private:
    QSettings settings_;
    Q_DISABLE_COPY_MOVE(InstanceSettings)
};

#endif // CLIENT_STORAGE_INSTANCE_SETTINGS_H
