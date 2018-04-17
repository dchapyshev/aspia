//
// PROJECT:         Aspia
// FILE:            client/ui/file_tree_widget.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_TREE_WIDGET_H
#define _ASPIA_CLIENT__UI__FILE_TREE_WIDGET_H

#include <QTreeWidget>

#include "client/file_transfer.h"

namespace aspia {

class FileItem;

class FileTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    FileTreeWidget(QWidget* parent = nullptr);
    ~FileTreeWidget();

signals:
    void fileListDroped(const QList<FileTransfer::Item>& file_list);

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supported_actions) override;

private:
    QPoint start_pos_;

    Q_DISABLE_COPY(FileTreeWidget)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_TREE_WIDGET_H
