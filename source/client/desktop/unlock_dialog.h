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

#ifndef CLIENT_DESKTOP_UNLOCK_DIALOG_H
#define CLIENT_DESKTOP_UNLOCK_DIALOG_H

#include <QDialog>
#include <QSet>

#include <memory>

#include "client/storage/database.h"

namespace Ui {
class UnlockDialog;
} // namespace Ui

// Lets the user choose a database or create one and unlocks it.
class UnlockDialog final : public QDialog
{
    Q_OBJECT

public:
    // In the LOCK mode the database is the one already chosen.
    enum class Mode { STARTUP, LOCK };
    Q_ENUM(Mode)

    explicit UnlockDialog(Mode mode, QWidget* parent = nullptr);
    ~UnlockDialog() final;

    // QDialog implementation.
    void reject() final;

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) final;
    void showEvent(QShowEvent* event) final;

private slots:
    void onDatabaseChanged();
    void onCreate();
    void onRename();
    void onDelete();

private:
    void reload();
    void unlock();
    void create();
    void rename();
    bool isNameAccepted(const QString& name, const QString& own_id);
    bool isCreating() const;
    bool isRenaming() const;
    void setCreating(bool creating);
    void requestNewPassword(const Database::Info& info);
    void setPasswordVisible(bool visible);

    std::unique_ptr<Ui::UnlockDialog> ui;
    const Mode mode_;
    QList<Database::Info> databases_;
    QSet<QString> open_ids_;
    QString no_password_id_;

    Q_DISABLE_COPY_MOVE(UnlockDialog)
};

#endif // CLIENT_DESKTOP_UNLOCK_DIALOG_H
