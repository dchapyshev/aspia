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

#ifndef HOST_UDP_ATTEMPT_H
#define HOST_UDP_ATTEMPT_H

#include <QObject>

#include <memory>

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/scoped_qpointer.h"
#include "proto/key_exchange.h"

class DatagramDecryptor;
class DatagramEncryptor;
class StunPeer;
class UdpChannel;
class UpnpPortMapper;

namespace proto::peer {
class HostToClient;
class UdpReply;
} // namespace proto::peer

// One UDP connection attempt over a single transport. Several derived attempts run concurrently;
// the first whose channel passes the bandwidth probe wins and the rest are torn down. The base
// owns the channel and the session crypto; each subclass implements one transport.
class UdpAttempt : public QObject
{
    Q_OBJECT

public:
    UdpAttempt(quint32 request_id, QObject* parent);
    ~UdpAttempt() override;

    quint32 requestId() const { return request_id_; }
    bool isValid() const;
    bool isConnected() const { return connected_; }

    // Hands the established channel over to the session (called when this attempt wins). Any aux
    // resource that must outlive the attempt (e.g. the UPnP mapping) is parented to the channel and
    // travels with it.
    ScopedQPointer<UdpChannel> takeChannel();

    // Sets up the transport resources and sends the request to the peer.
    virtual void start() = 0;

    // Handles the peer's reply for this attempt (crypto handshake + transport-specific wiring).
    virtual void onReply(const proto::peer::UdpReply& reply) = 0;

signals:
    void sig_connected(quint32 request_id);
    void sig_failed(quint32 request_id);
    void sig_message(const QByteArray& buffer);

protected:
    LOG_DECLARE_CONTEXT(UdpAttempt);

    // Creates the UDP channel and routes its signals to this attempt's signals.
    UdpChannel* createChannel();

    // Derives the session crypto from |reply|, installs it on the channel, and probes the channel
    // once it is ready. Returns false on failure.
    bool applyCrypto(const proto::peer::UdpReply& reply);

    // Fills the key-exchange fields common to every request message.
    template <class Request>
    void fillKeyExchange(Request* request)
    {
        request->set_request_id(request_id_);
        request->set_encryptions(proto::key_exchange::ENCRYPTION_AES256_GCM);
        request->set_public_key(key_pair_.publicKey().toStdString());
        request->set_iv(iv_.toStdString());
    }

    void sendMessage(const proto::peer::HostToClient& message);

    // Fails the attempt (emits sig_failed) if it has not connected within the timeout.
    void startTimeout();

    const quint32 request_id_;
    KeyPair key_pair_;
    QByteArray iv_;
    ScopedQPointer<UdpChannel> channel_;
    bool connected_ = false;

private:
    bool deriveCrypto(const proto::peer::UdpReply& reply,
                      std::unique_ptr<DatagramEncryptor>* encryptor,
                      std::unique_ptr<DatagramDecryptor>* decryptor);
    void onChannelReady();
    void onChannelMessage(quint8 channel_id, const QByteArray& buffer);
    void sendProbeIfReady();

    bool enet_ready_ = false;
    bool crypto_ready_ = false;
    bool probe_sent_ = false;

    Q_DISABLE_COPY_MOVE(UdpAttempt)
};

// Direct connection: binds locally and advertises the host's local addresses; client connects.
class DirectUdpAttempt final : public UdpAttempt
{
public:
    DirectUdpAttempt(quint32 request_id, QObject* parent);

    void start() final;
    void onReply(const proto::peer::UdpReply& reply) final;
};

// STUN hole punching: discovers the host's external endpoint, advertises it, and punches toward the
// client when the reply arrives.
class StunUdpAttempt final : public UdpAttempt
{
public:
    StunUdpAttempt(quint32 request_id, const QString& stun_host, quint16 stun_port, QObject* parent);
    ~StunUdpAttempt() final;

    void start() final;
    void onReply(const proto::peer::UdpReply& reply) final;

private:
    QString stun_host_;
    quint16 stun_port_ = 0;
    ScopedQPointer<StunPeer> stun_peer_;
};

// Host-side UPnP: opens a port mapping on the host's gateway and advertises it; client connects.
// The mapper is parented to the channel so the mapping lives as long as the (possibly won) channel.
class HostUpnpUdpAttempt final : public UdpAttempt
{
public:
    HostUpnpUdpAttempt(quint32 request_id, QObject* parent);

    void start() final;
    void onReply(const proto::peer::UdpReply& reply) final;
};

// Client-side UPnP: asks the client to open a mapping and connects to the endpoint it reports.
class ClientUpnpUdpAttempt final : public UdpAttempt
{
public:
    ClientUpnpUdpAttempt(quint32 request_id, QObject* parent);

    void start() final;
    void onReply(const proto::peer::UdpReply& reply) final;
};

#endif // HOST_UDP_ATTEMPT_H
