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
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT_UDP_ATTEMPT_H
#define CLIENT_UDP_ATTEMPT_H

#include <QObject>
#include <QStringList>

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/scoped_qpointer.h"

class StunPeer;
class UdpChannel;
class UpnpPortMapper;

// One client-side UDP connection attempt answering a single host request. Several run concurrently;
// the host probes the channel it picked, and the attempt that receives the probe wins. The base owns
// the channel and the session crypto; each subclass implements one transport.
class UdpAttempt : public QObject
{
    Q_OBJECT

public:
    UdpAttempt(quint32 request_id, const QByteArray& host_public_key, const QByteArray& host_iv,
               quint32 encryptions, QObject* parent);
    ~UdpAttempt() override;

    quint32 requestId() const { return request_id_; }
    bool isValid() const;

    // Hands the established channel over to the session (called when this attempt wins).
    ScopedQPointer<UdpChannel> takeChannel();

    // Sets up the transport resources and sends the reply to the host.
    virtual void start() = 0;

signals:
    void sig_connected(quint32 request_id);
    void sig_failed(quint32 request_id);
    void sig_message(const QByteArray& buffer);

protected:
    LOG_DECLARE_CONTEXT(UdpAttempt);

    // Creates the UDP channel, derives the session crypto, and routes the channel's signals to this
    // attempt's signals. Returns nullptr on crypto failure.
    UdpChannel* createChannel();

    // Sends the reply with this client's public key/IV, optionally carrying its external endpoint.
    void sendReply(const QString& external_address = QString(), quint16 external_port = 0);

    // Fails the attempt (emits sig_failed) if it has not connected within the timeout.
    void startTimeout();

    const quint32 request_id_;
    QByteArray host_public_key_;
    QByteArray host_iv_;
    quint32 encryptions_;
    KeyPair key_pair_;
    QByteArray iv_;
    ScopedQPointer<UdpChannel> channel_;
    bool connected_ = false;

private:
    // Detects the host's bandwidth probe, acknowledges it, and reports the win via sig_connected.
    void onChannelMessage(quint8 channel_id, const QByteArray& buffer);

    Q_DISABLE_COPY_MOVE(UdpAttempt)
};

// Direct/host-UPnP: connects to one of the host's advertised addresses.
class DirectUdpAttempt final : public UdpAttempt
{
public:
    DirectUdpAttempt(quint32 request_id, const QByteArray& host_public_key, const QByteArray& host_iv,
                     quint32 encryptions, const QStringList& addresses, quint16 port,
                     QObject* parent);

    void start() final;

private:
    QStringList addresses_;
    quint16 port_ = 0;
};

// STUN hole punching: discovers this client's external endpoint, connects to the host's external
// endpoint, and reports its own endpoint so the host can punch toward it.
class StunUdpAttempt final : public UdpAttempt
{
public:
    StunUdpAttempt(quint32 request_id, const QByteArray& host_public_key, const QByteArray& host_iv,
                   quint32 encryptions, const QString& host_address, quint16 host_port,
                   const QString& stun_host, quint16 stun_port, QObject* parent);
    ~StunUdpAttempt() final;

    void start() final;

private:
    QString host_address_;
    quint16 host_port_ = 0;
    QString stun_host_;
    quint16 stun_port_ = 0;
    ScopedQPointer<StunPeer> stun_peer_;
};

// Client-side UPnP: opens a port mapping on this client's gateway, listens, and reports the mapped
// endpoint so the host connects to it.
class UpnpUdpAttempt final : public UdpAttempt
{
public:
    UpnpUdpAttempt(quint32 request_id, const QByteArray& host_public_key, const QByteArray& host_iv,
                   quint32 encryptions, QObject* parent);

    void start() final;
};

#endif // CLIENT_UDP_ATTEMPT_H
