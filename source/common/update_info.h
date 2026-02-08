//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UPDATE_INFO_H
#define COMMON_UPDATE_INFO_H

#include <QByteArray>
#include <QString>
#include <QVersionNumber>

namespace common {

class UpdateInfo
{
public:
    UpdateInfo() = default;
    UpdateInfo(const UpdateInfo& other) = default;
    UpdateInfo& operator=(const UpdateInfo& other) = default;
    ~UpdateInfo() = default;

    static UpdateInfo fromXml(const QByteArray& buffer);

    bool isValid() const { return valid_; }
    const QVersionNumber& version() const { return version_; }
    const QString& description() const { return description_; }
    const QString& url() const { return url_; }

private:
    bool valid_ = false;
    QVersionNumber version_;
    QString description_;
    QString url_;
};

} // namespace common

#endif // COMMON_UPDATE_INFO_H
