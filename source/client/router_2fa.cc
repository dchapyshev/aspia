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

#include "client/router_2fa.h"

#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

#include "base/crypto/os_crypt.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "client/database.h"
#include "client/workers/router_worker.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
TwoFactorPrompt::TwoFactorPrompt(const QString& otpauth_uri, bool code_refused,
                                 qint64 blocked_seconds, QObject* parent)
    : QObject(parent),
      otpauth_uri_(otpauth_uri),
      code_refused_(code_refused),
      blocked_until_(blocked_seconds * 1000)
{
    // The wait ends on its own. The router has been counting the same block down since before
    // this side started, so when the announced seconds run out the question is announced again
    // and the dialog opens the regular way. A precise timer, because a coarse one may fire up
    // to a second early, and the announcement would find seconds still on the prompt and be
    // filtered by every gate that reads them.
    if (blocked_seconds > 0)
    {
        Router2FA* login = static_cast<Router2FA*>(parent);
        QTimer::singleShot(Seconds(blocked_seconds), Qt::PreciseTimer, this, [login]
        {
            emit login->sig_twoFactorRequired(login->config_->routerId());
        });
    }
}

//--------------------------------------------------------------------------------------------------
void TwoFactorPrompt::submitCode(const QString& totp_code)
{
    Router2FA* login = static_cast<Router2FA*>(parent());

    // Only the question being asked answers. A prompt already dying, or one that outlived its
    // question, must not send a code or spend the question that replaced it.
    if (login->prompt_ != this)
        return;

    login->sendCode(totp_code);
}

//--------------------------------------------------------------------------------------------------
Router2FA::Router2FA(SharedPointer<RouterConfig> config, QObject* parent)
    : QObject(parent),
      config_(std::move(config))
{
    LOG(INFO) << "Ctor";

    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
        LOG(ERROR) << "Router worker not found";
}

//--------------------------------------------------------------------------------------------------
Router2FA::~Router2FA()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Router2FA::onStart(const QVersionNumber& peer_version)
{
    LOG(INFO) << "Connected to router" << config_->address();
    version_ = peer_version;
    // The worker already unpaused the channel. The router speaks next with a challenge.
}

//--------------------------------------------------------------------------------------------------
void Router2FA::onConnectionLost(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Router connection error:" << error_code;

    // The question survives the connection. The worker keeps reconnecting, and the router asks
    // the same thing again.
    version_ = QVersionNumber();
}

//--------------------------------------------------------------------------------------------------
void Router2FA::onMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_CLIENT)
        return;

    proto::router::RouterToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse client message";
        return;
    }

    // Only the messages of the login are read here. Everything else belongs to the session
    // that does not exist yet.
    if (message.has_two_factor_challenge())
        readTwoFactorChallenge(message.two_factor_challenge());
    else if (message.has_login_result())
        readLoginResult(message.login_result());
    else
        LOG(WARNING) << "Unhandled client message";
}

//--------------------------------------------------------------------------------------------------
void Router2FA::openPrompt(const proto::router::TwoFactorChallenge& challenge,
                           const QString& otpauth_uri)
{
    // The seconds of the block come from the peer, and only their bounds are believed: a
    // negative block is no block, and none runs longer than the protocol allows.
    const qint64 blocked_seconds = std::clamp<qint64>(
        challenge.blocked_seconds(), 0, proto::router::kMaxTwoFactorBlockSeconds);

    // The router asks the same question after every reconnect. The prompt lives as long as the
    // question stands, so a challenge that asks what the open prompt already shows changes
    // nothing. The block is compared as a fact and not by the remaining seconds: both sides
    // count the same block down, and the repeated challenge carries no news in the remainder.
    if (prompt_ && prompt_->otpauthUri() == otpauth_uri &&
        prompt_->codeRefused() == challenge.code_rejected() &&
        (prompt_->blockedSeconds() > 0) == (blocked_seconds > 0))
    {
        return;
    }

    prompt_ = new TwoFactorPrompt(otpauth_uri, challenge.code_rejected(), blocked_seconds, this);
    emit sig_twoFactorRequired(config_->routerId());
}

//--------------------------------------------------------------------------------------------------
void Router2FA::sendCode(const QString& totp_code)
{
    // The channel lives on another thread, so no check made here can know whether the code will
    // reach the router. The answer is simply sent, and the question is closed on this side. The
    // truth comes back from the router alone: LoginResult when the code was accepted, or, when
    // it was refused or never arrived, the next challenge asking again and reopening the prompt.
    prompt_.reset();

    proto::router::ClientToRouter message;
    proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
    response->set_totp_code(totp_code.toStdString());
    send(message);
}

//--------------------------------------------------------------------------------------------------
void Router2FA::send(const proto::router::ClientToRouter& message)
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onSendMessage, Qt::QueuedConnection,
                              config_->routerId(), quint8(proto::router::CHANNEL_ID_CLIENT),
                              serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router2FA::readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge)
{
    switch (challenge.mode())
    {
        case proto::router::TWO_FACTOR_MODE_ACTIVE:
        {
            // The second ask means the token we presented was rejected (revoked, password
            // changed, database wiped). Presenting it again would loop forever, so the stale
            // copy goes and the operator is asked for a code.
            if (challenge.token_rejected())
            {
                LOG(INFO) << "Router rejected device token - clearing local copy";
                config_->clearDeviceToken();
                if (!Database::instance().modifyRouter(*config_))
                    LOG(WARNING) << "Failed to clear stale device token";

                openPrompt(challenge, QString());
                return;
            }

            // A token from a previous successful TOTP asks nobody. The login passes on its own.
            // The stored copy is wrapped by the OS keystore; one that does not open right now
            // (another user or machine, a locked keychain) stays in the record untouched and
            // the operator is asked for a code instead.
            const QByteArray wrapped = config_->deviceToken();
            QByteArray token;
            if (!wrapped.isEmpty() && !OSCrypt::decryptBytes(wrapped, &token))
                LOG(WARNING) << "Stored device token does not open here";
            if (!token.isEmpty())
            {
                proto::router::ClientToRouter message;
                proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
                response->set_token(token.toStdString());
                send(message);
                return;
            }

            LOG(INFO) << "Two-factor code required for router" << config_->routerId();
            openPrompt(challenge, QString());
            return;
        }

        case proto::router::TWO_FACTOR_MODE_ENROLL:
        {
            // What the operator scans comes from the peer, so it is checked before it reaches the
            // screen or displaces the stored token. A malformed challenge is refused whole, the
            // way an unknown mode is below. Nothing answers it, and the router times the silent
            // stage out.
            const std::string& otpauth_uri = challenge.otpauth_uri();
            if (!otpauth_uri.starts_with("otpauth://") ||
                otpauth_uri.size() > proto::router::kMaxOtpauthUriLength)
            {
                LOG(ERROR) << "Malformed otpauth URI for router" << config_->routerId();
                return;
            }

            // The dialogs show the secret of the query as the setup key. A URI without one
            // would put an empty key on screen and leave the operator with nothing to type.
            const QString uri = QString::fromStdString(otpauth_uri);
            if (QUrlQuery(QUrl(uri)).queryItemValue("secret").isEmpty())
            {
                LOG(ERROR) << "otpauth URI without a secret for router" << config_->routerId();
                return;
            }

            // Enrollment means a brand new TOTP secret, so a token of the previous account
            // life is dead. The challenge repeats on every reconnect of the stage.
            if (!config_->deviceToken().isEmpty())
            {
                config_->clearDeviceToken();
                if (!Database::instance().modifyRouter(*config_))
                    LOG(WARNING) << "Failed to clear stale device token";
            }

            LOG(INFO) << "Two-factor enrollment required for router" << config_->routerId();
            openPrompt(challenge, uri);
            return;
        }

        default:
            LOG(ERROR) << "Unknown TwoFactorMode:" << challenge.mode();
            return;
    }
}

//--------------------------------------------------------------------------------------------------
void Router2FA::readLoginResult(const proto::router::LoginResult& result)
{
    LOG(INFO) << "Login completed for router" << config_->routerId();

    // A token arrives only when a TOTP submission produced one. Failures drop the connection
    // instead of answering, so getting here at all means the session is open.
    const QByteArray new_token = QByteArray::fromStdString(result.new_token());
    if (new_token.size() == proto::router::kDeviceTokenSize)
    {
        LOG(INFO) << "Device token issued for router" << config_->routerId();

        // The record stores the token wrapped by the OS keystore. A failure to wrap costs the
        // token alone: the next login asks for a code again.
        QByteArray wrapped;
        if (OSCrypt::encryptBytes(new_token, &wrapped) && !wrapped.isEmpty())
        {
            config_->setDeviceToken(wrapped);
            if (!Database::instance().modifyRouter(*config_))
                LOG(WARNING) << "Failed to persist new device token for router" << config_->routerId();
        }
        else
        {
            LOG(WARNING) << "Failed to wrap new device token for router" << config_->routerId();
        }
    }
    else if (!new_token.isEmpty())
    {
        // The router issues tokens of exactly one size, so anything else is not a token, and
        // storing it would only buy a doomed round of presenting it back.
        LOG(WARNING) << "Device token of unexpected size for router" << config_->routerId();
    }

    // The login is over. The owner stops feeding the object on this report and only then
    // destroys it, so no late message can reach it.
    emit sig_twoFactorFinished(config_->routerId(), result.user_id(), version_);
}
