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

#ifndef ASPIA_SYSTEM_INFO__SERIALIZER__SERIALIZER_H_
#define ASPIA_SYSTEM_INFO__SERIALIZER__SERIALIZER_H_

#include <QObject>

#include <google/protobuf/message_lite.h>

namespace aspia {

class Serializer : public QObject
{
    Q_OBJECT

public:
    QString categoryUuid() const { return category_uuid_; }

    void setRequestUuid(const QString& uuid) { request_uuid_ = uuid; }
    QString requestUuid() const { return request_uuid_; }

    void setParams(const QByteArray& params) { params_ = params; }
    QByteArray params() const { return params_; }

public slots:
    virtual void start() = 0;

signals:
    void replyReady(const QString& request_uuid, const QByteArray& data);
    void finished();

protected:
    Serializer(QObject* parent, const QString& uuid);
    void onReady(const google::protobuf::MessageLite& message);

private:
    const QString category_uuid_;
    QString request_uuid_;
    QByteArray params_;
};

} // namespace aspia

#endif // ASPIA_SYSTEM_INFO__SERIALIZER__SERIALIZER_H_
