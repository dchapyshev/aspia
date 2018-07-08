//
// PROJECT:         Aspia
// FILE:            client/ui/system_info_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
#define _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H

#include "client/connect_data.h"
#include "ui_system_info_window.h"

namespace aspia {

class SystemInfoRequest;

class SystemInfoWindow : public QWidget
{
    Q_OBJECT

public:
    SystemInfoWindow(ConnectData* connect_data, QWidget* parent = nullptr);
    ~SystemInfoWindow() = default;

signals:
    void windowClose();
    void request(SystemInfoRequest* request);

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

private:
    Ui::SystemInfoWindow ui;

    QMenu* menu_;
    QLineEdit* search_edit_;

    Q_DISABLE_COPY(SystemInfoWindow)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
