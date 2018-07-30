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

#ifndef ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H_
#define ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H_

#include "base/macros_magic.h"
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

    DISALLOW_COPY_AND_ASSIGN(SystemInfoWindow);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H_
