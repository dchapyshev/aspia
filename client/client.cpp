/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/client.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/client.h"

#include <future>

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

Client::Client(std::unique_ptr<Socket> sock, OnEventCallback on_event_callback) :
    sock_(std::move(sock)),
    on_event_callback_(on_event_callback),
    encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    read_buffer_size_(0),
    write_buffer_size_(0)
{
    SetThreadPriority(Thread::Priority::Highest);
    Start();
}

Client::~Client()
{
    if (!IsThreadTerminated())
    {
        // Дожидаемся завершения потока.
        Stop();
        WaitForEnd();
    }
}

void Client::AsyncNotify(Client::Event type)
{
    std::async(std::launch::async, on_event_callback_, type);
}

void Client::DoKeyExchange()
{
    // Инициализируем шифратор и дешифратор.
    encryptor_.reset(new EncryptorAES());
    decryptor_.reset(new DecryptorAES());

    // Получаем размер публичного ключа и выделяем память для него.
    uint32_t public_key_len = decryptor_->GetPublicKeySize();
    std::unique_ptr<uint8_t[]> public_key(new uint8_t[public_key_len]);

    // Получаем публичный ключ дешифратора.
    decryptor_->GetPublicKey(public_key.get(), public_key_len);

    // Отправляем публичный ключ дешифратора.
    sock_->WriteMessage(public_key.get(), public_key_len);

    if (sock_->ReadMessageSize() != public_key_len)
    {
        LOG(ERROR) << "Wrong public key size recieved";
        throw Exception("Wrong public key size recieved.");
    }

    // Читаем публичный ключ для шифратора.
    sock_->ReadMessage(public_key.get(), public_key_len);

    // Устанавливаем публичный ключ для шифратора.
    encryptor_->SetPublicKey(public_key.get(), public_key_len);

    // Получаем размер сессионного ключа и выделяем память для него.
    uint32_t session_key_len = encryptor_->GetSessionKeySize();
    std::unique_ptr <uint8_t[]> session_key(new uint8_t[session_key_len]);

    if (sock_->ReadMessageSize() != session_key_len)
    {
        LOG(ERROR) << "Wrong session key size recieved";
        throw Exception("Wrong session key size recieved.");
    }

    // Читаем сессионный ключ дешифратора.
    sock_->ReadMessage(session_key.get(), session_key_len);

    // Устанавливаем сессионный ключ для дешифратора.
    decryptor_->SetSessionKey(session_key.get(), session_key_len);

    // Получаем сессионный ключ шифратора.
    encryptor_->GetSessionKey(session_key.get(), session_key_len);

    // Отправляем сессионный ключ шифратора.
    sock_->WriteMessage(session_key.get(), session_key_len);
}

void Client::WriteMessage(const proto::ClientToHost *message)
{
    try
    {
        uint32_t size = message->ByteSize();

        if (size)
        {
            LockGuard<Lock> guard(&write_lock_);

            if (write_buffer_size_ < size)
            {
                write_buffer_size_ = size;
                write_buffer_.reset(new ScopedAlignedBuffer(size));
            }

            if (!message->SerializeToArray(write_buffer_->get(), size))
            {
                LOG(ERROR) << "SerializeToArray() failed";
                throw Exception("Unable to serialize the message.");
            }

            uint8_t *encrypted_buffer = nullptr;

            encryptor_->Encrypt(write_buffer_->get(), size, &encrypted_buffer, &size);

            sock_->WriteMessage(encrypted_buffer, size);
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }
}

void Client::ReadMessage(std::unique_ptr<proto::HostToClient> *message)
{
    uint32_t size = sock_->ReadMessageSize();

    if (!size)
    {
        LOG(ERROR) << "ReadMessageSize() returns zero";
        throw Exception("Serialized message size is equal to zero.");
    }

    if (read_buffer_size_ < size)
    {
        read_buffer_size_ = size;
        read_buffer_.reset(new ScopedAlignedBuffer(size));
    }

    sock_->ReadMessage(read_buffer_->get(), size);

    uint8_t *buffer = nullptr;

    decryptor_->Decrypt(read_buffer_->get(), size, &buffer, &size);

    if (!message->get()->ParseFromArray(buffer, size))
    {
        LOG(ERROR) << "ParseFromArray() failed";
        throw Exception("Unable to parse the message.");
    }
}

void Client::EnableVideoUpdate(int32_t encoding,
                               const PixelFormat &pixel_format,
                               OnVideoUpdateCallback on_update_callback,
                               OnVideoResizeCallback on_resize_callback)
{
    on_video_update_callback_ = on_update_callback;
    on_video_resize_callback_ = on_resize_callback;

    SendVideoControl(true, encoding, pixel_format);
}

void Client::ReadVideoPacket(const proto::VideoPacket &packet)
{
    // Если пакет является первым в логическом обновлении
    if (packet.flags() & proto::VideoPacket::FIRST_PACKET)
    {
        // Переинициализируем декодер при необходимости
        if (encoding_ != packet.format().encoding())
        {
            encoding_ = packet.format().encoding();

            switch (encoding_)
            {
                case proto::VIDEO_ENCODING_VP8:
                    decoder_.reset(new VideoDecoderVP8());
                    break;

                case proto::VIDEO_ENCODING_ZLIB:
                    decoder_.reset(new VideoDecoderZLIB());
                    break;

                default:
                    LOG(ERROR) << "Unknown encoding: " << encoding_;
                    throw Exception("Unknown video encoding.");
                    break;
            }
        }
    }

    PixelFormat pixel_format;
    DesktopSize screen_size;
    uint8_t *buffer = nullptr;

    int32_t flags = decoder_->Decode(&packet, &buffer, changed_region_, screen_size, pixel_format);

    if (flags & proto::VideoPacket::FIRST_PACKET)
    {
        if (screen_size != screen_size_ || pixel_format != pixel_format_)
        {
            screen_size_ = screen_size;
            pixel_format_ = pixel_format;

            // Отправляем окну команду изменить размер.
            on_video_resize_callback_(screen_size, pixel_format);
        }
    }

    if (flags & proto::VideoPacket::LAST_PACKET)
    {
        // Сообщаем окну, что буфер изменился.
        on_video_update_callback_(buffer, changed_region_);

        changed_region_.Clear();
    }
}

void Client::SendVideoControl(bool enable,
                              int32_t encoding,
                              const PixelFormat &pixel_format)
{
    std::unique_ptr<proto::ClientToHost> message(new proto::ClientToHost());

    proto::VideoControl *video_control = message->mutable_video_control();

    video_control->set_enable(enable);
    video_control->set_encoding(encoding);

    //
    // Поле формата пикселей применимо на данный момент только для
    // данных кодировок, а для остальных - игнорируется.
    //
    if (encoding == proto::VIDEO_ENCODING_ZLIB)
    {
        proto::VideoPixelFormat *pf = video_control->mutable_pixel_format();

        pf->set_bits_per_pixel(pixel_format.BitsPerPixel());

        pf->set_red_max(pixel_format.RedMax());
        pf->set_green_max(pixel_format.GreenMax());
        pf->set_blue_max(pixel_format.BlueMax());

        pf->set_red_shift(pixel_format.RedShift());
        pf->set_green_shift(pixel_format.GreenShift());
        pf->set_blue_shift(pixel_format.BlueShift());
    }

    WriteMessage(message.get());
}

void Client::SendPointerEvent(int32_t x, int32_t y, int32_t mask)
{
    std::unique_ptr<proto::ClientToHost> message(new proto::ClientToHost());

    proto::PointerEvent *pointer_event = message->mutable_pointer_event();

    pointer_event->set_x(x);
    pointer_event->set_y(y);
    pointer_event->set_mask(mask);

    output_queue_->Add(message);
}

void Client::SendKeyEvent(int32_t keycode, bool extended, bool pressed)
{
    std::unique_ptr<proto::ClientToHost> message(new proto::ClientToHost());

    proto::KeyEvent *key_event = message->mutable_key_event();

    key_event->set_keycode(keycode);
    key_event->set_extended(extended);
    key_event->set_pressed(pressed);

    output_queue_->Add(message);
}

void Client::ProcessMessage(const proto::HostToClient *message)
{
    try
    {
        if (message->has_video_packet())
        {
            ReadVideoPacket(message->video_packet());
        }
        else if (message->has_cursor())
        {
            DLOG(ERROR) << "CursorShape unimplemented yet";
        }
        else if (message->has_clipboard())
        {
            DLOG(ERROR) << "Clipboard unimplemented yet";
        }
        else if (message->has_text_chat())
        {
            DLOG(ERROR) << "TextChat unimplemented yet";
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }
}

void Client::Worker()
{
    //
    // Обработка сообщения (в частности декодирование видео-пакетов) может занимать
    // длительное время. Для того, чтобы не блокировать поток чтения пакетов из сети
    // мы создаем отдельный поток для обработки сообщений. Сообщения добавляются в
    // очередь и обрабатываются в порядке поступления из данного потока.
    //
    MessageQueue<proto::HostToClient>::ProcessMessageCallback process_input_message_callback =
        std::bind(&Client::ProcessMessage, this, std::placeholders::_1);

    input_queue_.reset(new MessageQueue<proto::HostToClient>(process_input_message_callback));

    MessageQueue<proto::ClientToHost>::ProcessMessageCallback process_output_message_callback =
        std::bind(&Client::WriteMessage, this, std::placeholders::_1);

    // Для асинхронной отправки сообщений на сервер мы создаем очередь исходящик сообщений.
    output_queue_.reset(new MessageQueue<proto::ClientToHost>(process_output_message_callback));

    try
    {
        // Выполняем обмен ключами шифрования.
        DoKeyExchange();

        // Асинхронно вызываем callback для уведомления о том, что мы успешно подключились.
        AsyncNotify(Client::Event::Connected);

        // Продолжаем цикл пока не будет дана команда остановить поток.
        while (!IsThreadTerminating())
        {
            // Создаем экземпляр сообщения.
            std::unique_ptr<proto::HostToClient> message(new proto::HostToClient());

            // Читаем сообщение от сервера.
            ReadMessage(&message);

            // Добавляем сообщение в очередь.
            input_queue_->Add(message);
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }

    // Асинхронно вызываем callback для уведомления о том, что подключение завершено.
    AsyncNotify(Client::Event::Disconnected);
}

void Client::OnStop()
{
    // Отключаемся.
    sock_->Shutdown();
}

} // namespace aspia
