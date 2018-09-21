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

#ifndef ASPIA_HOST__HOST_SETTINGS_H_
#define ASPIA_HOST__HOST_SETTINGS_H_

#include <boost/property_tree/ptree.hpp>

#include "base/macros_magic.h"

namespace aspia {

struct SrpUserList;

class HostSettings
{
public:
    HostSettings();
    ~HostSettings();

    bool commit();

    std::string locale() const;
    void setLocale(const std::string& locale);

    uint16_t tcpPort() const;
    void setTcpPort(uint16_t port);

    std::shared_ptr<SrpUserList> userList() const;
    void setUserList(const SrpUserList& user_list);

private:
    boost::property_tree::ptree tree_;
    DISALLOW_COPY_AND_ASSIGN(HostSettings);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SETTINGS_H_
