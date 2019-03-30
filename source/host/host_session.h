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

#ifndef HOST__HOST_SESSION_H
#define HOST__HOST_SESSION_H

#include "base/macros_magic.h"

#include <QByteArray>
#include <QObject>

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class Session : public QObject
{
public:
    virtual ~Session() = default;

    static Session* create(const QString& session_type, const QString& channel_id);

    void start();
    void stop();

protected:
    explicit Session(const QString& channel_id);

    // Sends outgoing message.
    void sendMessage(const QByteArray& message);

    virtual void sessionStarted() = 0;
    virtual void messageReceived(const QByteArray& buffer) = 0;

private:
    QString channel_id_;
    ipc::Channel* channel_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Session);
};

} // namespace host

#endif // HOST__HOST_SESSION_H
