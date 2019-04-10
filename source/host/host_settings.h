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

#ifndef HOST__HOST_SETTINGS_H
#define HOST__HOST_SETTINGS_H

#include "base/macros_magic.h"

#include <QCoreApplication>
#include <QSettings>

namespace net {
class SrpUserList;
} // namespace net

namespace host {

class Settings
{
    Q_DECLARE_TR_FUNCTIONS(Settings)

public:
    Settings();
    ~Settings();

    static bool importFromFile(const QString& path, bool silent, QWidget* parent = nullptr);
    static bool exportToFile(const QString& path, bool silent, QWidget* parent = nullptr);

    QString filePath() const;
    bool isWritable() const;
    void sync();

    QString locale() const;
    void setLocale(const QString& locale);

    uint16_t tcpPort() const;
    void setTcpPort(uint16_t port);

    net::SrpUserList userList() const;
    void setUserList(const net::SrpUserList& user_list);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

private:
    static bool copySettings(
        const QString& source_path, const QString& target_path, bool silent, QWidget* parent);

    mutable QSettings system_settings_;
    QSettings user_settings_;

    DISALLOW_COPY_AND_ASSIGN(Settings);
};

} // namespace host

#endif // HOST__HOST_SETTINGS_H
