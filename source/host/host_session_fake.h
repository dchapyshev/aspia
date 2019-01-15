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

#ifndef HOST__HOST_SESSION_FAKE_H
#define HOST__HOST_SESSION_FAKE_H

#include <QObject>

#include "proto/common.pb.h"

namespace host {

class SessionFake : public QObject
{
    Q_OBJECT

public:
    virtual ~SessionFake() = default;

    static SessionFake* create(proto::SessionType session_type, QObject* parent);

    virtual void startSession() = 0;

signals:
    void sendMessage(const QByteArray& buffer);
    void errorOccurred();

public slots:
    virtual void onMessageReceived(const QByteArray& buffer) = 0;

protected:
    explicit SessionFake(QObject* parent);
};

} // namespace host

#endif // HOST__HOST_SESSION_FAKE_H
