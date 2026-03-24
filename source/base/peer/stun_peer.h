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

#ifndef BASE_PEER_STUN_PEER_H
#define BASE_PEER_STUN_PEER_H

#include <QHostAddress>
#include <QObject>

class QHostInfo;
class QSocketNotifier;
class QTimer;

namespace base {

class Location;

class StunPeer final : public QObject
{
    Q_OBJECT

public:
    explicit StunPeer(QObject* parent = nullptr);
    ~StunPeer();

    void start(const QString& stun_host, quint16 stun_port);
    qintptr takeSocket();

signals:
    void sig_channelReady(const QString& external_address, quint16 external_port);
    void sig_errorOccurred();

private slots:
    void onAttempt();

private:
    void doStart();
    void doStop();
    void onHostResolved(const QHostInfo& host_info);
    void onReadyRead();
    void onErrorOccurred(const Location& location);

    QTimer* timer_ = nullptr;
    int number_of_attempts_ = 0;

    QString stun_host_;
    quint16 stun_port_ = 0;

    qintptr socket_ = -1;
    QSocketNotifier* read_notifier_ = nullptr;
    quint32 transaction_id_ = 0;
    int lookup_id_ = -1;
    QHostAddress stun_address_;  // Resolved STUN server IPv4 address.

    qintptr ready_socket_ = -1;

    Q_DISABLE_COPY_MOVE(StunPeer)
};

} // namespace base

#endif // BASE_PEER_STUN_PEER_H
