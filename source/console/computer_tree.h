//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE_COMPUTER_TREE_H
#define CONSOLE_COMPUTER_TREE_H

#include <QTreeWidget>

#include "base/macros_magic.h"

namespace console {

class ComputerTree final : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ComputerTree(QWidget* parent = nullptr);
    ~ComputerTree() final = default;

    QString mimeType() const { return mime_type_; }

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void dragEnterEvent(QDragEnterEvent* event) final;
    void dragMoveEvent(QDragMoveEvent* event) final;
    void dropEvent(QDropEvent* event) final;
    void startDrag(Qt::DropActions supported_actions) final;

private slots:
    void onHeaderContextMenu(const QPoint& pos);

private:
    QPoint start_pos_;
    QString mime_type_;

    DISALLOW_COPY_AND_ASSIGN(ComputerTree);
};

} // namespace console

#endif // CONSOLE_COMPUTER_TREE_H
