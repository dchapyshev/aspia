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

#ifndef CONSOLE_COMPUTER_GROUP_TREE_H
#define CONSOLE_COMPUTER_GROUP_TREE_H

#include "console/computer_group_drag.h"

namespace console {

class ComputerGroupTree final : public QTreeWidget
{
    Q_OBJECT

public:
    ComputerGroupTree(QWidget* parent);
    ~ComputerGroupTree() final = default;

    void setComputerMimeType(const QString& mime_type);
    bool dragging() const;

signals:
    void sig_itemDropped();

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void dragEnterEvent(QDragEnterEvent* event) final;
    void dragLeaveEvent(QDragLeaveEvent* event) final;
    void dragMoveEvent(QDragMoveEvent* event) final;
    void dropEvent(QDropEvent* event) final;
    void startDrag(Qt::DropActions supported_actions) final;

private:
    bool isAllowedDropTarget(ComputerGroupItem* target, ComputerGroupItem* item);

    QString computer_mime_type_;
    QString computer_group_mime_type_;

    QPoint start_pos_;
    bool dragging_ = false;

    Q_DISABLE_COPY(ComputerGroupTree)
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_TREE_H
