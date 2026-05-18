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

#ifndef CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
#define CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QSet>

#include <memory>

class QAbstractButton;

namespace Ui {
class RouterWorkspaceDialog;
} // namespace Ui

class RouterWorkspaceDialog final : public QDialog
{
    Q_OBJECT

public:
    struct UserEntry
    {
        qint64 entry_id = 0;
        QString name;
        QByteArray public_key;
    };

    RouterWorkspaceDialog(qint64 entry_id, const QString& name, const QStringList& existing_names, QWidget* parent);
    ~RouterWorkspaceDialog() final;

    qint64 entryId() const { return entry_id_; }
    QString name() const { return name_; }
    const QSet<qint64>& accessUserIds() const { return access_user_ids_; }

    void setUsers(const QVector<UserEntry>& users);
    void setAccessUserIds(const QSet<qint64>& user_ids_with_access);

    // Self user_id is protected from revoke (in both create and modify modes).
    void setSelfUserId(qint64 user_id);

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void rebuildLists();
    void updateButtonsState();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 entry_id_ = -1;
    qint64 self_user_id_ = 0;
    QString name_;
    QStringList existing_names_;
    QHash<qint64, UserEntry> users_;
    QSet<qint64> access_user_ids_;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
