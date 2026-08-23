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

#ifndef CLIENT_ROUTER_2FA_H
#define CLIENT_ROUTER_2FA_H

#include <QDeadlineTimer>

#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"
#include "base/shared_pointer.h"
#include "client/config.h"
#include "proto/router_client.h"

class RouterWorker;

// The question of the two-factor stage of a login, and the way to answer it. Born when the
// router asks, it lives as long as the question stands, surviving the reconnects on the way,
// and dies with the answer or with a challenge that asks differently. Whoever shows it watches
// QObject::destroyed to learn the question is no longer asked.
class TwoFactorPrompt final : public QObject
{
    Q_OBJECT

public:
    // Empty when the account is enrolled and the operator just types the code; otherwise the
    // otpauth:// URI of a new secret they are to scan first.
    const QString& otpauthUri() const { return otpauth_uri_; }

    // The challenge said the last submitted code was refused, and the prompt says so to the
    // operator when it opens again.
    bool codeRefused() const { return code_refused_; }

    // Seconds left in the code block right now, zero when the account is not blocked. The
    // router does not look at codes while the block runs, so no dialog is opened and the
    // operator is shown the wait instead. The block was sent as a remaining duration, and
    // this side counts it down by itself.
    qint64 blockedSeconds() const { return (blocked_until_.remainingTime() + 999) / 1000; }

    // The answer of the operator. It destroys the prompt, and the reply of the router is
    // either LoginResult or the connection going down. A prompt that is no longer the question
    // being asked answers nothing.
    void submitCode(const QString& totp_code);

private:
    friend class Router2FA;
    TwoFactorPrompt(const QString& otpauth_uri, bool code_refused, qint64 blocked_seconds, QObject* parent);

    const QString otpauth_uri_;
    const bool code_refused_;
    const QDeadlineTimer blocked_until_;

    Q_DISABLE_COPY_MOVE(TwoFactorPrompt)
};

// The two-factor half of the login of a router record. The owner feeds it what the worker
// reports; it presents the stored device token or asks the operator through a TwoFactorPrompt,
// and reports the completed login with sig_twoFactorFinished. The object lives only while the
// record is logging in and survives its reconnects.
class Router2FA final : public QObject
{
    Q_OBJECT

public:
    explicit Router2FA(SharedPointer<RouterConfig> config, QObject* parent = nullptr);
    ~Router2FA() final;

    // The question of the running two-factor stage, nullptr while nothing is asked.
    TwoFactorPrompt* twoFactorPrompt() const { return prompt_; }

    const RouterConfig& config() const { return *config_; }

signals:
    void sig_twoFactorRequired(qint64 router_id);
    void sig_twoFactorFinished(qint64 router_id, qint64 user_id, const QVersionNumber& peer_version);

private slots:
    void onStart(const QVersionNumber& peer_version);
    void onConnectionLost(TcpChannel::ErrorCode error_code);
    void onMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    friend class Router2FATestPeer;
    friend class RouterController;
    friend class TwoFactorPrompt;

    void openPrompt(const proto::router::TwoFactorChallenge& challenge, const QString& otpauth_uri);
    void sendCode(const QString& totp_code);
    void reconnect();
    void send(const proto::router::ClientToRouter& message);
    void readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge);
    void readLoginResult(const proto::router::LoginResult& result);

    SharedPointer<RouterConfig> config_;
    QVersionNumber version_;
    QPointer<RouterWorker> router_worker_;
    ScopedQPointer<TwoFactorPrompt> prompt_;

    Q_DISABLE_COPY_MOVE(Router2FA)
};

#endif // CLIENT_ROUTER_2FA_H
