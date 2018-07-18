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

#ifndef _ASPIA_SYSTEM_INFO__UI__FORM_H
#define _ASPIA_SYSTEM_INFO__UI__FORM_H

#include <QWidget>

namespace aspia {

class SystemInfoRequest;

class Form : public QWidget
{
    Q_OBJECT

public:
    virtual ~Form() = default;

    QString uuid() const { return uuid_; }

public slots:
    virtual void readReply(const QString& uuid, const QByteArray& data) = 0;

signals:
    void sendRequest(SystemInfoRequest* request);

protected:
    Form(QWidget* parent, const QString& uuid)
        : QWidget(parent),
          uuid_(uuid)
    {
        // Nothing
    }

private:
    const QString uuid_;
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__UI__FORM_H
