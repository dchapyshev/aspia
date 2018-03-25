//
// PROJECT:         Aspia
// FILE:            client/ui/file_list_widget.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_LIST_WIDGET_H
#define _ASPIA_CLIENT__UI__FILE_LIST_WIDGET_H

#include <QTreeWidget>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileListWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit FileListWidget(QWidget* parent = nullptr);
    virtual ~FileListWidget();

signals:
    void addressChanged(const QString& subdirectory);

public slots:
    void updateFiles(const proto::file_transfer::FileList& file_list);

private slots:
    void fileDoubleClicked(QTreeWidgetItem* item, int column);

private:
    DISALLOW_COPY_AND_ASSIGN(FileListWidget);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_LIST_WIDGET_H
