/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/remote_client.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/remote_client.h"

#include "base/logging.h"

RemoteClient::RemoteClient(OnEventCallback on_event) :
    on_event_(on_event),
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    port_(0)
{
    // Nothing
}

RemoteClient::~RemoteClient()
{
    // Дожидаемся завершения потока.
    WaitForEnd();
}

void RemoteClient::ConnectTo(const char *hostname, int port)
{
    // Сохраняем адрес и порт.
    hostname_ = hostname;
    port_ = port;

    // Запускаем поток.
    Start();
}

void RemoteClient::Disconnect()
{
    // Даем команду потоку остановиться.
    Stop();
}

void RemoteClient::ReadVideoPacket(const proto::VideoPacket &packet)
{
    // Если пакет является первым в логическом обновлении
    if (packet.flags() & proto::VideoPacket::FIRST_PACKET)
    {
        // Получаем кодировку
        int32_t encoding = packet.format().encoding();

        // Переинициализируем декодер при необходимости
        if (current_encoding_ != encoding)
        {
            current_encoding_ = encoding;

            switch (encoding)
            {
                case proto::VIDEO_ENCODING_VP8:
                    decoder_.reset(new VideoDecoderVP8());
                    break;

                case proto::VIDEO_ENCODING_RAW:
                    decoder_.reset(new VideoDecoderRAW());
                    break;

                case proto::VIDEO_ENCODING_ZLIB:
                    decoder_.reset(new VideoDecoderZLIB());
                    break;

                default:
                    LOG(INFO) << "Unknown encoding: " << encoding;
                    break;
            }
        }

        if (VideoDecoder::IsResizeRequired(&packet))
        {
            pixel_format_ = VideoDecoder::GetPixelFormat(&packet);

            if (pixel_format_.is_empty())
                pixel_format_ = PixelFormat::MakeARGB();

            screen_size_ = VideoDecoder::GetScreenSize(&packet);

            if (decoder_)
            {
                decoder_->Resize(screen_size_, pixel_format_);
            }

            // Отправляем окну команду изменить размер.
            window_->Resize(screen_size_, pixel_format_);
        }
    }

    // Если декодер инициализирован.
    if (decoder_)
    {
        {
            // Получаем буфер, в который будем производить декодирование.
            ScreenWindowWin::LockedMemory memory = window_->GetBuffer();

            decoder_->Decode(&packet, memory.get());
        }

        // Сообщаем окну, что буфер изменился.
        window_->Invalidate();
    }
}

void RemoteClient::SendVideoControl(bool enable,
                                    int32_t encoding,
                                    const PixelFormat &pixel_format)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::VideoControl *ctrl = message->mutable_video_control();

    ctrl->set_enable(enable);
    ctrl->set_encoding(encoding);

    //
    // Поле формата пикселей применимо на данный момент только для
    // данных кодировок, а для остальных - игнорируется.
    //
    if (encoding == proto::VIDEO_ENCODING_ZLIB ||
        encoding == proto::VIDEO_ENCODING_RAW)
    {
        proto::VideoPixelFormat *pf = ctrl->mutable_pixel_format();

        pf->set_bits_per_pixel(pixel_format.bits_per_pixel());

        pf->set_red_max(pixel_format.red_max());
        pf->set_green_max(pixel_format.green_max());
        pf->set_blue_max(pixel_format.blue_max());

        pf->set_red_shift(pixel_format.red_shift());
        pf->set_green_shift(pixel_format.green_shift());
        pf->set_blue_shift(pixel_format.blue_shift());
    }

    socket_->WriteMessage(message.get());
}

void RemoteClient::ProcessInputMessage(const proto::ServerToClient *message)
{
    if (message->has_video_packet())
    {
        ReadVideoPacket(message->video_packet());
    }
    else if (message->has_cursor())
    {
        LOG(ERROR) << "CursorShape unimplemented yet";
    }
    else if (message->has_clipboard())
    {
        LOG(ERROR) << "Clipboard unimplemented yet";
    }
    else if (message->has_text_chat())
    {
        LOG(ERROR) << "TextChat unimplemented yet";
    }
}

void RemoteClient::OnScreenWindowClosed()
{
    if (window_)
    {
        window_->WaitForEnd();
        window_.reset();
    }

    Disconnect();
}

void RemoteClient::InsertToOutputQueue(std::unique_ptr<proto::ClientToServer> &message)
{
    output_message_queue_->Add(message);
}

void RemoteClient::ProcessOutputMessage(const proto::ClientToServer *message)
{
    try
    {
        socket_->WriteMessage(message);
    }
    catch (const Exception&)
    {
        Disconnect();
    }
}

void RemoteClient::InitScreenWindow()
{
    ScreenWindowWin::OnClosedCallback on_closed =
        std::bind(&RemoteClient::OnScreenWindowClosed, this);

    ScreenWindowWin::OnMessageAvailableCallback on_message =
        std::bind(&RemoteClient::InsertToOutputQueue, this, std::placeholders::_1);

    // Инициализируем его и запускаем поток.
    window_.reset(new ScreenWindowWin(nullptr, on_message, on_closed));

    // Запускаем поток окна.
    window_->Start();
}

void RemoteClient::InitInputQueue()
{
    MessageQueue<proto::ServerToClient>::ProcessMessageCallback process_input_message =
        std::bind(&RemoteClient::ProcessInputMessage, this, std::placeholders::_1);

    //
    // Обработка сообщения (в частности декодирование видео-пакетов) может занимать
    // длительное время. Для того, чтобы не блокировать поток чтения пакетов из сети
    // мы создаем отдельный поток для обработки сообщений. Сообщения добавляются в
    // очередь и обрабатываются в порядке поступления из данного потока.
    //
    input_message_queue_.reset(new MessageQueue<proto::ServerToClient>(process_input_message));

    // Запускаем поток обработки сообщений.
    input_message_queue_->Start();
}

void RemoteClient::InitOutputQueue()
{
    MessageQueue<proto::ClientToServer>::ProcessMessageCallback process_output_message =
        std::bind(&RemoteClient::ProcessOutputMessage, this, std::placeholders::_1);

    // Для асинхронной отправки сообщений на сервер мы создаем очередь исходящик сообщений.
    output_message_queue_.reset(new MessageQueue<proto::ClientToServer>(process_output_message));

    // Запускаем поток обработки сообщений.
    output_message_queue_->Start();
}

void RemoteClient::Worker()
{
    DLOG(INFO) << "Remote client thread started";

    try
    {
        // Создаем сокет.
        socket_.reset(new SocketTCP());

        // Пытаемся подключиться.
        socket_->Connect(hostname_.c_str(), port_);
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Unable to connect: " << err.What();

        //
        // Асинхронно вызываем callback для уведомления о том, что невозможно
        // подключиться.
        //
        std::async(std::launch::async, on_event_, EventType::NotConnected);

        // Выходим, поток завершен.
        return;
    }

    InitScreenWindow();
    InitInputQueue();
    InitOutputQueue();

    try
    {
        //
        // Асинхронно вызываем callback для уведомления о том, что мы успешно
        // подключились.
        //
        std::async(std::launch::async, on_event_, EventType::Connected);

        SendVideoControl(true, proto::VIDEO_ENCODING_VP8, PixelFormat::MakeRGB565());

        // Продолжаем цикл пока не будет дана команда остановить поток.
        while (!IsEndOfThread())
        {
            // Создаем экземпляр сообщения.
            std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

            // Читаем сообщение от сервера.
            socket_->ReadMessage(&message);

            // Добавляем сообщение в очередь.
            input_message_queue_->Add(message);
        }
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Exception in remote client thread: " << err.What();
    }

    if (window_)
    {
        window_->Stop();
        window_->WaitForEnd();
    }

    // Даем команду остановиться потокам обработки сообщений.
    input_message_queue_->Stop();
    output_message_queue_->Stop();

    // Дожидаемся остановки.
    input_message_queue_->WaitForEnd();
    output_message_queue_->WaitForEnd();

    // Асинхронно вызываем callback для уведомления о том, что подключение завершено.
    std::async(std::launch::async, on_event_, EventType::Disconnected);

    DLOG(INFO) << "Remote client thread stopped";
}

void RemoteClient::OnStop()
{
    // Если сокет был инициализирован.
    if (socket_)
    {
        // Отключаемся.
        socket_->Disconnect();
    }
}
