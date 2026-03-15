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

#ifndef BASE_NET_UDP_CHANNEL_H
#define BASE_NET_UDP_CHANNEL_H

#include <QObject>
#include <QQueue>

#include <memory>

#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

class QTimer;

namespace base {

class KcpSocket;
class Location;
class MessageDecryptor;
class MessageEncryptor;
class StunPeer;

class UdpChannel final : public QObject
{
    Q_OBJECT

public:
    explicit UdpChannel(QObject* parent = nullptr);
    ~UdpChannel() final;

    void connectTo(const QString& address, quint16 port);
    void send(quint8 channel_id, const QByteArray& buffer);
    bool isConnected() const;
    quint16 port() const;

    void setPaused(bool enable);

    void setEncryptor(std::unique_ptr<MessageEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<MessageDecryptor> decryptor);

signals:
    void sig_connected();
    void sig_errorOccurred();
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);

protected:
    friend class StunPeer;
    UdpChannel(asio::ip::udp::socket&& socket, QObject* parent);

private:
    struct Header
    {
        quint8 type;
        quint8 param1; // For USER_DATA: channel_id.
        quint8 param2; // Not used yet.
        quint8 param3; // Not used yet.
        quint32 length;
    };

    enum MessageType
    {
        KEEP_ALIVE = 1,
        USER_DATA  = 2
    };

    enum KeepAliveFlags
    {
        KEEP_ALIVE_PONG = 0,
        KEEP_ALIVE_PING = 1
    };

    enum KeepAliveTimerType
    {
        KEEP_ALIVE_TIMEOUT = 0,
        KEEP_ALIVE_INTERVAL = 1
    };

    class WriteTask
    {
    public:
        WriteTask(quint8 type, quint8 param, const QByteArray& data)
            : type_(type),
              param_(param),
              data_(data)
        {
            // Nothing
        }

        WriteTask(const WriteTask& other) = default;
        WriteTask& operator=(const WriteTask& other) = default;

        quint8 type() const { return type_; }
        quint8 param() const { return param_; }
        const QByteArray& data() const { return data_; }

    private:
        quint8 type_;
        quint8 param_;
        QByteArray data_;
    };

    void init();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void addWriteTask(quint8 type, quint8 param, const QByteArray& data);
    void doWrite();
    void doRead();
    bool processKcpRecv();
    bool onMessageReceived(int offset);
    void onKeepAliveTimer();

    asio::ip::udp::resolver resolver_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;

    KcpSocket* kcp_socket_ = nullptr;
    QTimer* keep_alive_timer_;
    KeepAliveTimerType keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    QByteArray keep_alive_counter_;

    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    QQueue<WriteTask> write_queue_;
    QByteArray write_buffer_;

    QByteArray read_buffer_;
    QByteArray decrypt_buffer_;

    bool connected_ = false;
    bool paused_ = true;
    bool reading_ = false;

    static const int kRecvBufferSize = 65536;
    std::array<char, kRecvBufferSize> recv_buffer_;

    Q_DISABLE_COPY_MOVE(UdpChannel)
};

} // namespace base

#endif // BASE_NET_UDP_CHANNEL_H
