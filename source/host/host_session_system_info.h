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

#ifndef ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H_
#define ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H_

#include "host/host_session.h"
#include "protocol/system_info_session.pb.h"
#include "system_info/category.h"

namespace aspia {

class HostSessionSystemInfo : public HostSession
{
    Q_OBJECT

public:
    explicit HostSessionSystemInfo(const QString& channel_id);
    ~HostSessionSystemInfo() = default;

public slots:
    // HostSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;

protected:
    void startSession() override;
    void stopSession() override;

private:
    void readCategoryListRequest(const std::string& request_uuid,
                                 const proto::system_info::CategoryListRequest& request);
    void readCategoryRequest(const std::string& request_uuid,
                             const proto::system_info::CategoryRequest& request);

    QList<Category> category_list_;

    Q_DISABLE_COPY(HostSessionSystemInfo)
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H_
