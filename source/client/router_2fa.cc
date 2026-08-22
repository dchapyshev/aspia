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

#include "base/core_application.h"
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
      blocked_seconds_(blocked_seconds)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void TwoFactorPrompt::submitCode(const QString& totp_code)
{
    // The prompt answers once. It is already dying when the answer is handed in, and a repeated
    // call must not send a second code.
    if (answered_)
        return;
    answered_ = true;

    static_cast<Router2FA*>(parent())->sendCode(totp_code);
}

//--------------------------------------------------------------------------------------------------
Router2FA::Router2FA(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config)
{
    LOG(INFO) << "Ctor";

    // The interface runs on GuiApplication, the headless tools on CoreApplication.
    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
        router_worker_ = CoreApplication::findWorker<RouterWorker>();

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
    LOG(INFO) << "Connected to router" << config_.address();
    version_ = peer_version;
    // The worker already unpaused the channel. The router speaks next with a challenge or
    // LoginResult.
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
    // The router asks the same question after every reconnect. The prompt lives as long as the
    // question stands, so a challenge that asks what the open prompt already shows changes
    // nothing.
    if (prompt_ && prompt_->otpauthUri() == otpauth_uri &&
        prompt_->codeRefused() == challenge.code_rejected() &&
        prompt_->blockedSeconds() == challenge.blocked_seconds())
    {
        return;
    }

    prompt_ = new TwoFactorPrompt(otpauth_uri, challenge.code_rejected(),
                                  challenge.blocked_seconds(), this);
    emit sig_twoFactorRequired(config_.routerId());
}

//--------------------------------------------------------------------------------------------------
void Router2FA::sendCode(const QString& totp_code)
{
    // The question is answered. The reply is either LoginResult or the connection going down,
    // and the next challenge asks anew.
    prompt_.reset();

    proto::router::ClientToRouter message;
    proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
    response->set_totp_code(totp_code.toStdString());
    send(message);
}

//--------------------------------------------------------------------------------------------------
void Router2FA::reconnect()
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onReconnect, Qt::QueuedConnection,
                              config_.routerId());
}

//--------------------------------------------------------------------------------------------------
void Router2FA::send(const proto::router::ClientToRouter& message)
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onSendMessage, Qt::QueuedConnection,
                              config_.routerId(), quint8(proto::router::CHANNEL_ID_CLIENT),
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
                config_.clearDeviceToken();
                if (!Database::instance().modifyRouter(config_))
                    LOG(WARNING) << "Failed to clear stale device token";

                openPrompt(challenge, QString());
                return;
            }

            // A token from a previous successful TOTP asks nobody. The login passes on its own.
            const QByteArray token = config_.deviceToken();
            if (!token.isEmpty())
            {
                proto::router::ClientToRouter message;
                proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
                response->set_token(token.toStdString());
                send(message);
                return;
            }

            LOG(INFO) << "Two-factor code required for router" << config_.routerId();
            openPrompt(challenge, QString());
            return;
        }

        case proto::router::TWO_FACTOR_MODE_ENROLL:
        {
            // What the operator scans comes from the peer, so it is checked before it reaches the
            // screen or displaces the stored token. A malformed challenge is refused whole, the
            // way an unknown mode is below.
            const std::string& otpauth_uri = challenge.otpauth_uri();
            if (!otpauth_uri.starts_with("otpauth://") ||
                otpauth_uri.size() > proto::router::kMaxOtpauthUriLength)
            {
                LOG(ERROR) << "Malformed otpauth URI for router" << config_.routerId();
                reconnect();
                return;
            }

            // Enrollment means a brand new TOTP secret, so a token of the previous account
            // life is dead.
            config_.clearDeviceToken();
            if (!Database::instance().modifyRouter(config_))
                LOG(WARNING) << "Failed to clear stale device token";

            LOG(INFO) << "Two-factor enrollment required for router" << config_.routerId();
            openPrompt(challenge, QString::fromStdString(otpauth_uri));
            return;
        }

        default:
            LOG(ERROR) << "Unknown TwoFactorMode:" << challenge.mode();
            reconnect();
            return;
    }
}

//--------------------------------------------------------------------------------------------------
void Router2FA::readLoginResult(const proto::router::LoginResult& result)
{
    LOG(INFO) << "Login completed for router" << config_.routerId();

    // A token arrives only when a TOTP submission produced one. Failures drop the connection
    // instead of answering, so getting here at all means the session is open.
    const QByteArray new_token = QByteArray::fromStdString(result.new_token());
    if (!new_token.isEmpty())
    {
        LOG(INFO) << "Device token issued for router" << config_.routerId();

        config_.setDeviceToken(new_token);
        if (!Database::instance().modifyRouter(config_))
            LOG(WARNING) << "Failed to persist new device token for router" << config_.routerId();
    }

    // The login is over. The owner stops feeding the object on this report and only then
    // destroys it, so no late message can reach it.
    emit sig_twoFactorFinished(config_.routerId(), result.user_id(), version_);
}
