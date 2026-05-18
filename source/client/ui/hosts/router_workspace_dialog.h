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

#include <memory>

class QAbstractButton;

namespace Ui {
class RouterWorkspaceDialog;
} // namespace Ui

class RouterWorkspaceDialog final : public QDialog
{
    Q_OBJECT

public:
    RouterWorkspaceDialog(qint64 entry_id,
                          const QString& name,
                          const QStringList& existing_names,
                          QWidget* parent);
    ~RouterWorkspaceDialog() final;

    qint64 entryId() const { return entry_id_; }
    QString name() const { return name_; }

private:
    void onButtonBoxClicked(QAbstractButton* button);

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 entry_id_ = -1;
    QString name_;
    QStringList existing_names_;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
