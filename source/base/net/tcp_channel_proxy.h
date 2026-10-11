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

#ifndef BASE_NET_TCP_CHANNEL_PROXY_H
#define BASE_NET_TCP_CHANNEL_PROXY_H

#include <QUrl>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include <map>
#include <string_view>

#include "base/net/curl_util.h"
#include "base/net/proxy_settings.h"
#include "base/net/tcp_channel.h"

class TcpChannelProxy final : public TcpChannel
{
    Q_OBJECT

public:
    ~TcpChannelProxy() final;

    // A channel through the proxy when |settings| is enabled, or a direct one otherwise.
    static TcpChannel* create(Type type, const ProxySettings& settings, Authenticator* authenticator,
                              QObject* parent = nullptr);

    // TcpChannel implementation.
    void connectTo(const QString& address, quint16 port, Seconds timeout) final;
    bool setReadBufferSize(int size) final;
    bool setWriteBufferSize(int size) final;

protected:
    // TcpChannel implementation.
    void readTransport(void* data, size_t size) final;
    void writeTransport(const void* data, size_t size) final;
    void closeTransport() final;

private:
    // Private, so a channel never goes through a proxy the user turned off.
    TcpChannelProxy(Type type, const ProxySettings& settings, Authenticator* authenticator, QObject* parent);

    struct CurlSocket
    {
        explicit CurlSocket(asio::io_context& io_context)
            : socket(io_context)
        {
            // Nothing
        }

        bool& waiting(asio::socket_base::wait_type type)
        {
            switch (type)
            {
                case asio::socket_base::wait_read: return read_waiting;
                case asio::socket_base::wait_write: return write_waiting;
                default: return error_waiting;
            }
        }

        asio::ip::tcp::socket socket;
        int what = 0;
        bool read_waiting = false;
        bool write_waiting = false;
        bool error_waiting = false;
    };

    static curl_socket_t openSocketCallback(void* clientp, curlsocktype purpose, curl_sockaddr* address);
    static int closeSocketCallback(void* clientp, curl_socket_t socket);
    static int socketCallback(CURL* easy, curl_socket_t socket, int what, void* userp, void* socketp);
    static int timerCallback(CURLM* multi, long timeout_ms, void* userp);
    static int debugCallback(CURL* easy, curl_infotype type, char* data, size_t size, void* clientp);

    void resolveProxy();
    void resolvePeer();
    bool onResolved(
        const QUrl& url, const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints);
    void startTransfer();
    curl_socket_t openSocket(int family);
    int closeSocket(curl_socket_t socket);
    void watchSocket(curl_socket_t socket, int what);
    void waitSocket(const SharedPointer<CurlSocket>& curl_socket, asio::socket_base::wait_type type);
    void setMultiTimeout(long timeout_ms);
    void driveMulti(curl_socket_t socket, int action);
    void onConnectDone(CURLcode result);
    void continueRead();
    void continueWrite();
    void onTransportError(CURLcode result);
    void postError(ErrorCode error_code);
    void onDebugMessage(curl_infotype type, std::string_view message);

    asio::io_context& io_context_;
    asio::ip::tcp::resolver resolver_;
    asio::steady_timer connect_timer_;
    asio::steady_timer multi_timer_;
    ScopedCURLM multi_;

    // Declared before |curl_|, which reads it.
    ScopedCurlSlist resolve_list_;

    ScopedCURL curl_;

    const ProxySettings settings_;
    QUrl proxy_url_;
    QUrl peer_url_;
    std::string peer_address_;

    // Sockets that curl opened through openSocket and has not closed yet.
    std::map<curl_socket_t, SharedPointer<CurlSocket>> sockets_;
    SharedPointer<CurlSocket> tunnel_;

    char* read_data_ = nullptr;
    size_t read_size_ = 0;
    size_t read_done_ = 0;

    const char* write_data_ = nullptr;
    size_t write_size_ = 0;
    size_t write_done_ = 0;

    Q_DISABLE_COPY_MOVE(TcpChannelProxy)
};

#endif // BASE_NET_TCP_CHANNEL_PROXY_H
