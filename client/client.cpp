//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"

#include <future>

#include "base/clipboard.h"
#include "base/exception.h"
#include "base/logging.h"

#include "crypto/sha512.h"

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
    if (IsActiveThread())
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
        throw Exception("Wrong public key size recieved");

    // Читаем публичный ключ для шифратора.
    sock_->ReadMessage(public_key.get(), public_key_len);

    // Устанавливаем публичный ключ для шифратора.
    encryptor_->SetPublicKey(public_key.get(), public_key_len);

    // Получаем размер сессионного ключа и выделяем память для него.
    uint32_t session_key_len = encryptor_->GetSessionKeySize();
    std::unique_ptr <uint8_t[]> session_key(new uint8_t[session_key_len]);

    if (sock_->ReadMessageSize() != session_key_len)
        throw Exception("Wrong session key size recieved");

    // Читаем сессионный ключ дешифратора.
    sock_->ReadMessage(session_key.get(), session_key_len);

    // Устанавливаем сессионный ключ для дешифратора.
    decryptor_->SetSessionKey(session_key.get(), session_key_len);

    // Получаем сессионный ключ шифратора.
    encryptor_->GetSessionKey(session_key.get(), session_key_len);

    // Отправляем сессионный ключ шифратора.
    sock_->WriteMessage(session_key.get(), session_key_len);
}

void Client::DoAuthorize(const std::string &username, const std::string &password)
{
    
}

void Client::WriteMessage(const proto::ClientToHost *message)
{
    try
    {
        uint32_t size = message->ByteSize();

        if (size)
        {
            if (write_buffer_size_ < size)
            {
                write_buffer_size_ = size;
                write_buffer_.resize(size);
            }

            if (!message->SerializeToArray(write_buffer_.get(), size))
                throw Exception("Unable to serialize the message");

            uint8_t *encrypted_buffer = nullptr;

            encryptor_->Encrypt(write_buffer_.get(), size, &encrypted_buffer, &size);

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

    if (size)
    {
        if (read_buffer_size_ < size)
        {
            read_buffer_size_ = size;
            read_buffer_.resize(size);
        }

        sock_->ReadMessage(read_buffer_.get(), size);

        uint8_t *buffer = nullptr;

        decryptor_->Decrypt(read_buffer_.get(), size, &buffer, &size);

        if (!message->get()->ParseFromArray(buffer, size))
            throw Exception("Unable to parse the message");
    }
}

void Client::StartScreenUpdate(const ScreenConfig &config,
                               OnVideoUpdateCallback on_update_callback,
                               OnVideoResizeCallback on_resize_callback,
                               OnCursorUpdateCallback on_cursor_update_callback)
{
    on_video_update_callback_  = on_update_callback;
    on_video_resize_callback_  = on_resize_callback;
    on_cursor_update_callback_ = on_cursor_update_callback;

    ApplyScreenConfig(config);
}

void Client::EndScreenUpdate()
{
    on_video_update_callback_  = nullptr;
    on_video_resize_callback_  = nullptr;
    on_cursor_update_callback_ = nullptr;

    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::VideoControl *video_control = msg->mutable_control()->mutable_video();
    video_control->set_flags(proto::VideoControl::DISABLE_VIDEO);

    output_queue_->Add(msg);
}

void Client::ApplyScreenConfig(const ScreenConfig &config)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    uint32_t flags = proto::VideoControl::ENABLE_VIDEO;

    flags |= config.DisableDesktopEffects() ? proto::VideoControl::DISABLE_DESKTOP_EFFECTS : 0;
    flags |= config.ShowRemoteCursor() ? proto::VideoControl::ENABLE_CURSOR_SHAPE : 0;

    proto::VideoControl *video_control = msg->mutable_control()->mutable_video();

    video_control->set_flags(flags);
    video_control->set_encoding(config.Encoding());
    video_control->set_update_interval(config.UpdateInterval());

    //
    // Поле формата пикселей применимо на данный момент только для
    // данных кодировок, а для остальных - игнорируется.
    //
    if (config.Encoding() == proto::VIDEO_ENCODING_ZLIB)
    {
        video_control->set_compress_ratio(config.CompressRatio());
        config.Format().ToVideoPixelFormat(video_control->mutable_pixel_format());
    }

    output_queue_->Add(msg);
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
                    video_decoder_.reset(new VideoDecoderVP8());
                    break;

                case proto::VIDEO_ENCODING_VP9:
                    video_decoder_.reset(new VideoDecoderVP9());
                    break;

                case proto::VIDEO_ENCODING_ZLIB:
                    video_decoder_.reset(new VideoDecoderZLIB());
                    break;

                default:
                    LOG(ERROR) << "Unknown encoding: " << encoding_;
                    throw Exception("Unknown video encoding");
                    break;
            }
        }
    }

    PixelFormat pixel_format;
    DesktopSize screen_size;
    uint8_t *buffer = nullptr;

    int32_t flags = video_decoder_->Decode(&packet, &buffer, changed_region_, screen_size, pixel_format);

    if (flags & proto::VideoPacket::FIRST_PACKET)
    {
        if (screen_size != screen_size_ || pixel_format != pixel_format_)
        {
            screen_size_ = screen_size;
            pixel_format_ = pixel_format;

            if (!on_video_resize_callback_._Empty())
            {
                // Отправляем окну команду изменить размер.
                on_video_resize_callback_(screen_size, pixel_format);
            }
        }
    }

    // TODO: Сделать настройку обновления по последнему пакету.
    if (flags & proto::VideoPacket::LAST_PACKET)
    {
        if (!on_video_update_callback_._Empty())
        {
            // Сообщаем окну, что буфер изменился.
            on_video_update_callback_(buffer, changed_region_);
        }

        changed_region_.Clear();
    }
}

void Client::ReadCursorShape(const proto::CursorShape &msg)
{
    // Если декодер курсора не инициализирован.
    if (!cursor_decoder_)
    {
        // Инициализируем его.
        cursor_decoder_.reset(new CursorDecoder());
    }

    // Декодируем изображение курсора.
    const MouseCursor *mouse_cursor = cursor_decoder_->Decode(msg);

    if (!on_cursor_update_callback_._Empty() && mouse_cursor)
    {
        // Вызываем callback для уведомления об изменении курсора.
        on_cursor_update_callback_(mouse_cursor);
    }
}

void Client::ReadClipboard(const proto::Clipboard &msg)
{
    Clipboard::Set(msg.data());
}

void Client::SendPointerEvent(int32_t x, int32_t y, uint32_t mask)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::PointerEvent *event = msg->mutable_pointer_event();

    event->set_x(x);
    event->set_y(y);
    event->set_mask(mask);

    output_queue_->Add(msg);
}

void Client::SendKeyEvent(uint32_t keycode, uint32_t flags)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::KeyEvent *event = msg->mutable_key_event();

    event->set_keycode(keycode);
    event->set_flags(flags);

    output_queue_->Add(msg);
}

void Client::SendBellEvent()
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::BellEvent *event = msg->mutable_bell_event();

    event->set_flags(0);

    output_queue_->Add(msg);
}

void Client::SendPowerControl(proto::PowerControl::Action action)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::PowerControl *control = msg->mutable_control()->mutable_power();

    control->set_action(action);

    output_queue_->Add(msg);
}

void Client::SendClipboardControl(uint32_t flags)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::ClipboardControl *control = msg->mutable_control()->mutable_clipboard();

    control->set_flags(flags);

    output_queue_->Add(msg);
}

void Client::SendClipboard(const std::string &data)
{
    std::unique_ptr<proto::ClientToHost> msg(new proto::ClientToHost());

    proto::Clipboard *clipboard = msg->mutable_clipboard();

    clipboard->set_data(data);

    output_queue_->Add(msg);
}

void Client::ProcessMessage(const proto::HostToClient *message)
{
    try
    {
        if (message->has_video_packet())
        {
            ReadVideoPacket(message->video_packet());
        }

        if (message->has_cursor())
        {
            ReadCursorShape(message->cursor());
        }

        if (message->has_clipboard())
        {
            ReadClipboard(message->clipboard());
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
