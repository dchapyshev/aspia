//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session.h"

namespace aspia {

ClientSession::ClientSession(std::unique_ptr<Socket> sock) :
    sock_(std::move(sock))
{
    // Nothing
}

bool ClientSession::DoKeyExchange()
{
    // Инициализируем шифратор и дешифратор.
    encryptor_.reset(new EncryptorAES());
    decryptor_.reset(new DecryptorAES());

    if (!decryptor_->Init() || !encryptor_->Init())
        return false;

    // Получаем размер публичного ключа и выделяем память для него.
    uint32_t public_key_len = decryptor_->GetPublicKeySize();
    std::unique_ptr<uint8_t[]> public_key(new uint8_t[public_key_len]);

    // Получаем публичный ключ дешифратора.
    if (!decryptor_->GetPublicKey(public_key.get(), public_key_len))
        return false;

    // Отправляем публичный ключ дешифратора.
    if (!sock_->Write(public_key.get(), public_key_len))
        return false;

    if (sock_->ReadSize() != public_key_len)
        return false;

    // Читаем публичный ключ для шифратора.
    if (!sock_->Read(public_key.get(), public_key_len))
        return false;

    // Устанавливаем публичный ключ для шифратора.
    if (!encryptor_->SetPublicKey(public_key.get(), public_key_len))
        return false;

    // Получаем размер сессионного ключа и выделяем память для него.
    uint32_t session_key_len = encryptor_->GetSessionKeySize();
    std::unique_ptr <uint8_t[]> session_key(new uint8_t[session_key_len]);

    if (sock_->ReadSize() != session_key_len)
        return false;

    // Читаем сессионный ключ дешифратора.
    if (!sock_->Read(session_key.get(), session_key_len))
        return false;

    // Устанавливаем сессионный ключ для дешифратора.
    if (!decryptor_->SetSessionKey(session_key.get(), session_key_len))
        return false;

    // Получаем сессионный ключ шифратора.
    if (!encryptor_->GetSessionKey(session_key.get(), session_key_len))
        return false;

    // Отправляем сессионный ключ шифратора.
    if (!sock_->Write(session_key.get(), session_key_len))
        return false;

    return true;
}

ClientSession::SessionEvent ClientSession::DoAuthorization()
{
    std::string username;
    std::string password;

    if (!OnAuthorizationRequest(username, password))
    {
        DLOG(INFO) << "Authorization canceled by user";
        return SessionEvent::CANCELED;
    }

    return SessionEvent::OPEN;
}

void ClientSession::OpenSession()
{
    Start(Thread::Priority::AboveNormal);
}

void ClientSession::CloseSession()
{
    Stop();
    Wait();
}

bool ClientSession::WriteOutgoingMessage(const uint8_t* buffer, uint32_t size)
{
    const uint8_t* encrypted_buffer = encryptor_->Encrypt(buffer, size, &size);

    if (!encrypted_buffer || !sock_->Write(encrypted_buffer, size))
    {
        Stop();
        return false;
    }

    return true;
}

void ClientSession::Worker()
{
    // Выполняем обмен ключами шифрования.
    if (!DoKeyExchange())
    {
        Stop();

        // Во время обмена ключами шифрования произошла ошибка, выходим.
        OnSessionEvent(SessionEvent::KEY_EXCHANGE_ERROR);
        return;
    }

    // Выполняем авторизацию.
    SessionEvent event = DoAuthorization();
    if (event != SessionEvent::OPEN)
    {
        Stop();

        // Во время авторизации произошла ошибка, выходим.
        OnSessionEvent(event);
        return;
    }

    // Уведомляем об открытии сессии.
    OnSessionEvent(SessionEvent::OPEN);

    // Продолжаем цикл пока не будет дана команда остановить поток.
    while (!IsTerminating())
    {
        uint32_t size = sock_->ReadSize();

        if (!size)
            break;

        if (incomming_buffer_.Size() < size)
            incomming_buffer_.Resize(size);

        if (!sock_->Read(incomming_buffer_, size))
            break;

        const uint8_t* buffer = decryptor_->Decrypt(incomming_buffer_, size, &size);
        if (!buffer)
            break;

        if (!OnIncommingMessage(buffer, size))
            break;
    }

    // Уведомляем о закрытии сессии.
    OnSessionEvent(SessionEvent::CLOSE);

    Stop();
}

void ClientSession::OnStop()
{
    // Отключаемся.
    sock_->Disconnect();
}

} // namespace aspia
