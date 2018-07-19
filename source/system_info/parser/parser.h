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

#ifndef ASPIA_SYSTEM_INFO__UI__PARSER_H_
#define ASPIA_SYSTEM_INFO__UI__PARSER_H_

#include <QIcon>
#include <QObject>
#include <QJsonObject>
#include <QtXml>

namespace aspia {

class SystemInfoRequest;

class Parser : public QObject
{
public:
    virtual ~Parser() = default;

    virtual QJsonObject toJson() = 0;
    virtual QDomElement toXml() = 0;
    virtual QDomElement toHtml() = 0;
    virtual QString toText() = 0;

    QString uuid() const { return uuid_; }

public slots:
    virtual void updateData() const = 0;
    virtual void readReply(const QString& uuid, const QByteArray& data) = 0;

signals:
    void sendRequest(SystemInfoRequest* request);
    void dataUpdated();

protected:
    Parser(QObject* parent, const QString& uuid)
        : QObject(parent),
          uuid_(uuid)
    {
        // Nothing
    }

private:
    QString uuid_;
};

} // namespace aspia

#endif // ASPIA_SYSTEM_INFO__UI__PARSER_H_
