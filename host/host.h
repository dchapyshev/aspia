//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_H
#define _ASPIA_HOST__HOST_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/thread.h"
#include "crypto/encryptor_rsa_aes.h"
#include "crypto/decryptor_rsa_aes.h"
#include "network/socket_tcp.h"

#include "host/desktop_session_server.h"

namespace aspia {

class Host : public Thread
{
public:
    enum class SessionEvent
    {
        OPEN,               // Сессия успешно открыта.
        CLOSE,              // Сессия закрыта.
        KEY_EXCHANGE_ERROR, // Ошибка обмена ключами шифрования.
        ACCESS_DENIED       // Доступ запрещен.
    };

    typedef std::function<void(SessionEvent)> EventCallback;

    Host(std::unique_ptr<Socket> sock, const EventCallback& event_callback);

    virtual ~Host() = default;

    bool IsDead();

private:
    void Worker() override;
    void OnStop() override;

    bool DoKeyExchange();
    bool WriteMessage(const uint8_t* buffer, uint32_t size);

private:
    EventCallback event_callback_;

    std::unique_ptr<Socket> sock_;

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

    ScopedAlignedBuffer read_buffer_;

    DesktopSessionServer session_;
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_H
