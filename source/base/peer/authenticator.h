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

#ifndef BASE_PEER_AUTHENTICATOR_H
#define BASE_PEER_AUTHENTICATOR_H

#include <QVersionNumber>

#include "base/crypto/generic_hash.h"

class Location;
class QTimer;

namespace proto::key_exchange {
enum Encryption : int;
enum Identify : int;
} // namespace proto::key_exchange

class Authenticator : public QObject
{
    Q_OBJECT

public:
    explicit Authenticator(QObject* parent);
    virtual ~Authenticator() override;

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };
    Q_ENUM(State)

    enum class ErrorCode
    {
        SUCCESS,
        UNKNOWN_ERROR,
        PROTOCOL_ERROR,
        VERSION_ERROR,
        ACCESS_DENIED,
        SESSION_DENIED
    };
    Q_ENUM(ErrorCode)

    void start();

    [[nodiscard]] proto::key_exchange::Identify identify() const { return identify_; }
    [[nodiscard]] proto::key_exchange::Encryption encryption() const { return encryption_; }
    [[nodiscard]] const QVersionNumber& peerVersion() const { return peer_version_; }
    [[nodiscard]] const QString& peerOsName() const { return peer_os_name_; }
    [[nodiscard]] const QString& peerComputerName() const { return peer_computer_name_; }
    [[nodiscard]] const QString& peerArch() const { return peer_arch_; }
    [[nodiscard]] const QString& peerDisplayName() const { return peer_display_name_; }
    [[nodiscard]] quint32 sessionType() const { return session_type_; }
    [[nodiscard]] const QString& userName() const { return user_name_; }
    [[nodiscard]] bool isProbe() const { return is_probe_; }

    // Returns the current state.
    [[nodiscard]] State state() const { return state_; }

    // The session key is the digest of every secret and handshake message that was fed into the
    // transcript hash via appendTranscript(). Channels read it after sig_keyChanged(). Returns an
    // empty array until setSessionKeyReady() has been called, so accidental reads before the handshake
    // has finalized the key fail early instead of silently producing a partial-state digest.
    [[nodiscard]] QByteArray sessionKey() const;
    [[nodiscard]] const QByteArray& encryptIv() const { return encrypt_iv_; }
    [[nodiscard]] const QByteArray& decryptIv() const { return decrypt_iv_; }

public slots:
    void onIncomingMessage(const QByteArray& data);

signals:
    void sig_outgoingMessage(const QByteArray& data, bool encrypted);
    void sig_keyChanged();
    void sig_finished(Authenticator::ErrorCode error_code);

protected:
    LOG_DECLARE_CONTEXT(Authenticator);

    [[nodiscard]] virtual bool onStarted() = 0;
    virtual void onReceived(const QByteArray& buffer) = 0;

    // Feeds key material (handshake bytes and/or raw shared secrets) into the running BLAKE2s256
    // accumulator that produces the session key. Order must match on both peers. NG authenticators
    // feed handshake messages plus secrets; Legacy authenticators feed only raw secrets so that
    // the resulting key matches pre-transcript-binding wire compatibility.
    void appendTranscript(const QByteArray& data);

    // Marks the transcript as finalized: subsequent sessionKey() calls return the digest, and the
    // sig_keyChanged signal is emitted so channels can install their encryptor/decryptor.
    void setSessionKeyReady();

    void finish(const Location& location, ErrorCode error_code);
    void setPeerVersion(const proto::peer::Version& version);
    void setPeerOsName(const QString& name);
    void setPeerComputerName(const QString& name);
    void setPeerArch(const QString& arch);
    void setPeerDisplayName(const QString& display_name);

    proto::key_exchange::Encryption encryption_;
    proto::key_exchange::Identify identify_;
    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    quint32 session_type_ = 0; // Selected session type.
    QString user_name_;
    bool is_probe_ = false;

private:
    GenericHash transcript_hash_;
    bool key_ready_ = false;
    QTimer* timer_ = nullptr;
    State state_ = State::STOPPED;
    QVersionNumber peer_version_; // Remote peer version.
    QString peer_os_name_;
    QString peer_computer_name_;
    QString peer_arch_;
    QString peer_display_name_;
};

Q_DECLARE_METATYPE(Authenticator::ErrorCode)

#endif // BASE_PEER_AUTHENTICATOR_H
