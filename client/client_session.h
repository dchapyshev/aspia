//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/scoped_aligned_buffer.h"
#include "network/socket_tcp.h"
#include "crypto/encryptor_rsa_aes.h"
#include "crypto/decryptor_rsa_aes.h"

namespace aspia {

class ClientSession : public Thread
{
public:
    explicit ClientSession(std::unique_ptr<Socket> sock);
    virtual ~ClientSession() = default;

    enum class SessionEvent
    {
        OPEN,               // Сессия успешно открыта.
        CLOSE,              // Сессия закрыта.
        CANCELED,           // Отменено пользователем.
        KEY_EXCHANGE_ERROR, // Ошибка обмена ключами шифрования.
        ACCESS_DENIED       // Доступ запрещен.
    };

    virtual void OnSessionEvent(SessionEvent event) = 0;
    virtual bool OnAuthorizationRequest(std::string& username, std::string& password) = 0;
    virtual bool OnIncommingMessage(const uint8_t* buffer, uint32_t size) = 0;

    void OpenSession();
    void CloseSession();
    bool WriteOutgoingMessage(const uint8_t* buffer, uint32_t size);

private:
    bool DoKeyExchange();
    SessionEvent DoAuthorization();

    void Worker() override;
    void OnStop() override;

private:
    std::unique_ptr<Socket> sock_;

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

    ScopedAlignedBuffer incomming_buffer_;

    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
