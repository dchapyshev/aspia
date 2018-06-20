//
// PROJECT:         Aspia
// FILE:            client/ui/file_manager_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H
#define _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H

#include "client/connect_data.h"
#include "ui_file_manager_window.h"

namespace aspia {

class FileManagerWindow : public QWidget
{
    Q_OBJECT

public:
    FileManagerWindow(ConnectData* connect_data, QWidget* parent = nullptr);
    ~FileManagerWindow() = default;

public slots:
    void refresh();

signals:
    void windowClose();
    void localRequest(FileRequest* request);
    void remoteRequest(FileRequest* request);

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void removeItems(FilePanel* sender, const QList<FileRemover::Item>& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender, const QList<FileTransfer::Item>& items);

private:
    void transferItems(FileTransfer::Type type,
                       const QString& source_path,
                       const QString& target_path,
                       const QList<FileTransfer::Item>& items);

    Ui::FileManagerWindow ui;

    Q_DISABLE_COPY(FileManagerWindow)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H
