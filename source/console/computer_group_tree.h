//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE__COMPUTER_GROUP_TREE_H
#define CONSOLE__COMPUTER_GROUP_TREE_H

#include "console/computer_group_drag.h"

namespace console {

class ComputerGroupTree : public QTreeWidget
{
    Q_OBJECT

public:
    ComputerGroupTree(QWidget* parent);
    ~ComputerGroupTree() = default;

    void setComputerMimeType(const QString& mime_type);
    bool dragging() const;

signals:
    void itemDropped();

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supported_actions) override;

private:
    bool isAllowedDropTarget(ComputerGroupItem* target, ComputerGroupItem* item);

    QString computer_mime_type_;
    QString computer_group_mime_type_;

    QPoint start_pos_;
    bool dragging_ = false;

    DISALLOW_COPY_AND_ASSIGN(ComputerGroupTree);
};

} // namespace console

#endif // CONSOLE__COMPUTER_GROUP_TREE_H
