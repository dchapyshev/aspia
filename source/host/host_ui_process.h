//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__HOST_UI_PROCESS_H
#define HOST__HOST_UI_PROCESS_H

#include "base/macros_magic.h"
#include "base/win/session_id.h"
#include "base/win/process.h"
#include "proto/notifier.pb.h"

#include <QPointer>

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class UiProcess : public QObject
{
    Q_OBJECT

public:
    explicit UiProcess(ipc::Channel* channel, QObject* parent = nullptr);
    ~UiProcess();

    static void create(base::win::SessionId session_id);

    base::win::SessionId sessionId() const;

    void setConnectEvent(const proto::notifier::ConnectEvent& event);
    void setDisconnectEvent(const std::string& uuid);

    enum class State { STOPPED, STARTED };

    State state() const { return state_; }

    bool start();
    void stop();

signals:
    void started();
    void finished();
    void killSession(const std::string& session_uuid);

private slots:
    void onProcessFinished(int exit_code);
    void onMessageReceived(const QByteArray& buffer);

private:
    // Contains the status of the UI process.
    State state_ = State::STOPPED;

    // The channel is used to communicate with the UI process.
    ipc::Channel* channel_;

    base::win::Process* process_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UiProcess);
};

} // namespace host

#endif // HOST__HOST_UI_PROCESS_H
