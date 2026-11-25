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

#ifndef CLIENT_DESKTOP_DESKTOP_SCRIPT_OUTPUT_DIALOG_H
#define CLIENT_DESKTOP_DESKTOP_SCRIPT_OUTPUT_DIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class ScriptOutputDialog;
} // namespace Ui

// Shows what a script started on the remote computer prints.
class ScriptOutputDialog final : public QDialog
{
    Q_OBJECT

public:
    ScriptOutputDialog(qint32 script_id, QWidget* parent = nullptr);
    ~ScriptOutputDialog() final;

    // Grid of the terminal the output is shown in.
    static const int kColumns = 120;
    static const int kRows = 30;

    qint32 scriptId() const { return script_id_; }
    bool isFinished() const { return finished_; }

    void appendOutput(const QByteArray& data);
    void setFinished();

private:
    std::unique_ptr<Ui::ScriptOutputDialog> ui;

    const qint32 script_id_;
    bool finished_ = false;

    Q_DISABLE_COPY_MOVE(ScriptOutputDialog)
};

#endif // CLIENT_DESKTOP_DESKTOP_SCRIPT_OUTPUT_DIALOG_H
