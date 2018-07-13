//
// PROJECT:         Aspia
// FILE:            client/ui/system_info_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
#define _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H

#include "client/connect_data.h"
#include "protocol/system_info_session.pb.h"
#include "ui_system_info_window.h"

namespace aspia {

class SystemInfoRequest;

class SystemInfoWindow : public QWidget
{
    Q_OBJECT

public:
    SystemInfoWindow(ConnectData* connect_data, QWidget* parent = nullptr);
    ~SystemInfoWindow() = default;

public slots:
    void refresh();

signals:
    void windowClose();
    void newRequest(SystemInfoRequest* request);

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCategoryListReceived(const proto::system_info::Reply& reply);

private:
    Ui::SystemInfoWindow ui;

    QMenu* menu_;
    QLineEdit* search_edit_;

    Q_DISABLE_COPY(SystemInfoWindow)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
