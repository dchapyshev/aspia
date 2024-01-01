//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER_SESSION_H
#define ROUTER_SESSION_H

#include "base/version.h"
#include "base/net/tcp_channel.h"
#include "base/memory/local_memory.h"
#include "proto/router_common.pb.h"

namespace router {

class Database;
class DatabaseFactory;
class Server;
class SharedKeyPool;

class Session : public base::TcpChannel::Listener
{
public:
    explicit Session(proto::RouterSession session_type);
    virtual ~Session() override;

    using SessionId = int64_t;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionFinished(
            SessionId session_id, proto::RouterSession session_type) = 0;
    };

    void setChannel(std::unique_ptr<base::TcpChannel> channel);
    void setRelayKeyPool(std::unique_ptr<SharedKeyPool> relay_key_pool);
    void setDatabaseFactory(base::local_shared_ptr<DatabaseFactory> database_factory);
    void setServer(Server* server);

    void start(Delegate* delegate);

    void setVersion(const base::Version& version);
    const base::Version& version() const { return version_; }
    void setOsName(const std::string& os_name);
    const std::string& osName() const { return os_name_; }
    void setComputerName(const std::string& computer_name);
    const std::string& computerName() const { return computer_name_; }
    void setArchitecture(const std::string& architecture);
    const std::string& architecture() const { return architecture_; }
    void setUserName(const std::string& username);
    const std::string& userName() const { return username_; }

    proto::RouterSession sessionType() const { return session_type_; }
    SessionId sessionId() const { return session_id_; }
    const std::string& address() const { return address_; }
    time_t startTime() const { return start_time_; }
    std::chrono::seconds duration() const;

protected:
    void sendMessage(uint8_t channel_id, const google::protobuf::MessageLite& message);
    std::unique_ptr<Database> openDatabase() const;

    virtual void onSessionReady() = 0;
    virtual void onSessionMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) = 0;
    virtual void onSessionMessageWritten(uint8_t channel_id, size_t pending) = 0;

    // base::TcpChannel::Listener implementation.
    void onTcpConnected() final;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) final;
    void onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) final;
    void onTcpMessageWritten(uint8_t channel_id, base::ByteArray&& buffer, size_t pending) final;

    SharedKeyPool& relayKeyPool() { return *relay_key_pool_; }
    const SharedKeyPool& relayKeyPool() const { return *relay_key_pool_; }

    Server& server() { return *server_; }
    const Server& server() const { return *server_; }

private:
    const proto::RouterSession session_type_;
    const SessionId session_id_;
    time_t start_time_ = 0;

    std::unique_ptr<base::TcpChannel> channel_;
    base::local_shared_ptr<DatabaseFactory> database_factory_;
    std::unique_ptr<SharedKeyPool> relay_key_pool_;
    Server* server_ = nullptr;

    std::string address_;
    std::string username_;
    base::Version version_;
    std::string os_name_;
    std::string computer_name_;
    std::string architecture_;

    Delegate* delegate_ = nullptr;
};

} // namespace router

#endif // ROUTER_SESSION_H
