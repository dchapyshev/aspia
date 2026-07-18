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

#ifndef CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_DIALOG_H
#define CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_DIALOG_H

#include <QDialog>

#include <memory>

class QAbstractButton;

namespace Ui {
class LocalHostDialog;
} // namespace Ui

class LocalHostDialog final : public QDialog
{
    Q_OBJECT

public:
    LocalHostDialog(qint64 entry_id, qint64 group_id, QWidget* parent = nullptr);
    ~LocalHostDialog() final;

    qint64 entryId() const { return entry_id_; }

private slots:
    void onRouterChanged(int index);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void updateAddressLabel();

    std::unique_ptr<Ui::LocalHostDialog> ui;
    qint64 entry_id_ = -1;
    qint64 group_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalHostDialog)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_LOCAL_HOST_DIALOG_H
