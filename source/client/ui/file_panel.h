//
// PROJECT:         Aspia
// FILE:            client/ui/file_panel.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_PANEL_H
#define _ASPIA_CLIENT__UI__FILE_PANEL_H

#include "base/macros.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"
#include "qt/ui_file_panel.h"

namespace aspia {

class FilePanel : public QWidget
{
    Q_OBJECT

public:
    FilePanel(QWidget* parent = nullptr);
    ~FilePanel();

    void setPanelName(const QString& name);

    QString currentPath() const;
    void setCurrentPath(const QString& path);

signals:
    void request(const proto::file_transfer::Request& request,
                 const FileReplyReceiver& receiver);

public slots:
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);
    void refresh();

private slots:
    void addressItemChanged(int index);
    void fileDoubleClicked(QTreeWidgetItem* item, int column);
    void fileSelectionChanged();
    void fileItemChanged(QTreeWidgetItem* item, int column);
    void toChildFolder(const QString& child_name);
    void toParentFolder();
    void addFolder();
    void removeSelected();
    void sendSelected();

private:
    QString addressItemPath(int index) const;
    void updateDrives(const proto::file_transfer::DriveList& list);
    void updateFiles(const proto::file_transfer::FileList& list);

    Ui::FilePanel ui;

    QString current_path_;

    DISALLOW_COPY_AND_ASSIGN(FilePanel);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_PANEL_H
