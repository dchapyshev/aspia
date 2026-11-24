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
class InstanceSettings
{
public:
    InstanceSettings();
    ~InstanceSettings();

    static void select(const QString& id);
    static void remove(const QString& id);

    static QString filePath();
    static void setFilePathForTesting(const QString& file_path);

    QVariantMap tabState(const QString& name) const;
    void setTabState(const QString& name, const QVariantMap& state);

    QStringList quickConnectHistory() const;
    void setQuickConnectHistory(const QStringList& history);

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
