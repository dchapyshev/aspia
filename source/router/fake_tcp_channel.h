//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER_FAKE_TCP_CHANNEL_H
#define ROUTER_FAKE_TCP_CHANNEL_H

#include <QList>

#include "base/net/tcp_channel.h"

// A channel that keeps what the session sent and hands the session what a peer would have sent.
// TcpChannel is already the interface both real implementations sit behind, so a session driven
// through this one behaves exactly as it does on a socket - minus the socket. The peer identity is
// what the authenticator established before the session was created; a session never takes it
// from the messages it receives.
class FakeTcpChannel final : public TcpChannel
{
    Q_OBJECT

public:
    explicit FakeTcpChannel(QObject* parent = nullptr)
        : TcpChannel(Type::DIRECT, parent)
    {
        // Nothing
    }

    ~FakeTcpChannel() final = default;

    struct Sent
    {
        quint8 channel_id = 0;
        QByteArray buffer;
    };

    void setPeer(qint64 user_id, const std::string& user_name, quint32 session_type,
                 const QVersionNumber& version, const std::string& address = "203.0.113.5")
    {
        user_id_ = user_id;
        user_name_ = user_name;
        session_type_ = session_type;
        version_ = version;
        address_ = address;
        computer_name_ = "CONSOLE";
        architecture_ = "x86_64";
        os_name_ = "Windows 11";
    }

    // Delivers a message to the session, as the socket would: a paused channel does not read,
    // so the frame waits until the pause is lifted.
    void receive(quint8 channel_id, const QByteArray& buffer)
    {
        if (paused_)
        {
            pending_.append({ channel_id, buffer });
            return;
        }
        emit sig_messageReceived(channel_id, buffer);
    }

    const QList<Sent>& sent() const { return sent_; }
    bool nothingSent() const { return sent_.isEmpty(); }
    void clearSent() { sent_.clear(); }

    // TcpChannel implementation.
    void connectTo(const QString& /* address */, quint16 /* port */, Seconds /* timeout */) final {}
    bool isConnected() const final { return true; }
    bool isAuthenticated() const final { return true; }
    bool isPaused() const final { return paused_; }

    // Lifting the pause hands over what arrived during it. Delivery can pause the channel again
    // (a session closing mid-queue), and the rest keeps waiting the way pending reads do.
    void setPaused(bool enable) final
    {
        paused_ = enable;
        while (!paused_ && !pending_.isEmpty())
        {
            const Sent frame = pending_.takeFirst();
            emit sig_messageReceived(frame.channel_id, frame.buffer);
        }
    }
    void send(quint8 channel_id, const QByteArray& buffer) final { sent_.append({ channel_id, buffer }); }
    bool setReadBufferSize(int /* size */) final { return true; }
    bool setWriteBufferSize(int /* size */) final { return true; }
    qint64 pendingBytes() const final { return 0; }
    void tick(TimePoint /* now */) final {}

protected:
    void doAuthentication() final {}

private:
    QList<Sent> sent_;
    QList<Sent> pending_;
    bool paused_ = true;

    Q_DISABLE_COPY_MOVE(FakeTcpChannel)
};

#endif // ROUTER_FAKE_TCP_CHANNEL_H
