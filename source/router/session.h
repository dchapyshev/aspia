//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__SESSION_H
#define ROUTER__SESSION_H

#include "base/version.h"
#include "base/net/network_channel.h"

namespace router {

class Database;
class DatabaseFactory;

class Session : public base::NetworkChannel::Listener
{
public:
    Session(std::unique_ptr<base::NetworkChannel> channel,
            std::shared_ptr<DatabaseFactory> database_factory);
    virtual ~Session();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionFinished() = 0;
    };

    void start(Delegate* delegate);

    bool isFinished() const;

    void setVersion(const base::Version& version);
    const base::Version& version() const { return version_; }
    void setUserName(const std::u16string& username);
    const std::u16string& userName() const { return username_; }

protected:
    void send(base::ByteArray&& buffer);
    std::unique_ptr<Database> openDatabase() const;

    // net::Channel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;

private:
    std::unique_ptr<base::NetworkChannel> channel_;
    std::shared_ptr<DatabaseFactory> database_factory_;

    std::u16string username_;
    base::Version version_;

    Delegate* delegate_ = nullptr;
};

} // namespace router

#endif // ROUTER__SESSION_H
