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

namespace proto::peer {
class DirectUdpRequest;
class GatewayUdpRequest;
class StunUdpRequest;
} // namespace proto::peer

enum class UdpMethod;

class GatewayPortMapper;
class StunPeer;
class UdpChannel;

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

    // The transport this attempt represents, reported in the session statistics.
    virtual UdpMethod method() const = 0;

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

// Direct/host-gateway: connects to one of the host's advertised addresses. |gateway| marks an
// endpoint the host mapped on its own gateway.
class DirectUdpAttempt final : public UdpAttempt
{
public:
    DirectUdpAttempt(const proto::peer::DirectUdpRequest& request, QObject* parent);

    void start() final;
    UdpMethod method() const final;

private:
    QStringList addresses_;
    quint16 port_ = 0;
    const bool gateway_;
};

// STUN hole punching: discovers this client's external endpoint, connects to the host's external
// endpoint, and reports its own endpoint so the host can punch toward it.
class StunUdpAttempt final : public UdpAttempt
{
public:
    StunUdpAttempt(const proto::peer::StunUdpRequest& request, QObject* parent);
    ~StunUdpAttempt() final;

    void start() final;
    UdpMethod method() const final;

private:
    QString host_address_;
    quint16 host_port_ = 0;
    QString stun_host_;
    quint16 stun_port_ = 0;
    ScopedQPointer<StunPeer> stun_peer_;
};

// Client-side gateway mapping: opens a port mapping on this client's gateway (PCP, NAT-PMP or UPnP),
// listens, and reports the mapped endpoint so the host connects to it. |methods| selects which of
// the gateway-mapping protocols are allowed.
class GatewayUdpAttempt final : public UdpAttempt
{
public:
    GatewayUdpAttempt(const proto::peer::GatewayUdpRequest& request, quint32 methods, QObject* parent);

    void start() final;
    UdpMethod method() const final;

private:
    const quint32 methods_;
};

#endif // CLIENT_UDP_ATTEMPT_H
