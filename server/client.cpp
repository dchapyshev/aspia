/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/client.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "server/client.h"

#include "base/logging.h"

Client::Client(std::unique_ptr<Socket> socket,
               uint32_t client_id,
               OnDisconnectedCallback on_client_disconnected) :
    socket_(std::move(socket)),
    client_id_(client_id),
    on_client_disconnected_(on_client_disconnected),
    feature_mask_(proto::FEATURE_DESKTOP_MANAGE)
{
    DCHECK(socket_);

    // Если сессия не имеет разрешенных возможностей.
    if (feature_mask_ == proto::FEATURE_NONE)
    {
        // Вызываем исключение.
        throw Exception("Client session has no features.");
    }
}

Client::~Client()
{
    // Nothing
}

uint32_t Client::GetID() const
{
    return client_id_;
}

void Client::WriteMessage(const proto::ServerToClient *message)
{
    socket_->WriteMessage(message);
}

void Client::Worker()
{
    DLOG(INFO) << "Client thread started";

    // Если сессия имеет возможность управления рабочим столом.
    if (feature_mask_ & proto::FEATURE_DESKTOP_MANAGE)
    {
        // Инициализируем обработчик ввода.
        input_handler_.reset(new InputHandler());
    }

    try
    {
        std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

        // Продолжаем цикл пока не будет дана команда остановить поток.
        while (!IsEndOfThread())
        {
            message->Clear();

            // Читаем сообщение от клиента.
            socket_->ReadMessage(&message);

            if (message->has_pointer_event())
            {
                ReadPointerEvent(message->pointer_event());
            }
            else if (message->has_key_event())
            {
                ReadKeyEvent(message->key_event());
            }
            else if (message->has_video_control())
            {
                ReadVideoControl(message->video_control());
            }
            else if (message->has_cursor_shape_control())
            {
                LOG(ERROR) << "CursorShapeControl unimplemented yet";
            }
            else if (message->has_clipboard())
            {
                LOG(ERROR) << "Clipboard unimplemented yet";
            }
            else if (message->has_clipboard_request())
            {
                LOG(ERROR) << "ClipboardRequest unimplemented yet";
            }
            else if (message->has_clipboard_control())
            {
                LOG(ERROR) << "ClipboardControl unimplemented yet";
            }
            else if (message->has_power_control())
            {
                LOG(ERROR) << "PowerControl unimplemented yet";
            }
            else if (message->has_bell())
            {
                LOG(ERROR) << "Bell unimplemented yet";
            }
            else if (message->has_text_chat())
            {
                LOG(ERROR) << "TextChat unimplemented yet";
            }
        }
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Exception in client thread: " << err.What();
    }

    // Если поток отправки видео-пакетов был инициализирован.
    if (screen_sender_)
    {
        // Даем ему команду остановиться.
        screen_sender_->Stop();

        // Дожидаемся остановки.
        screen_sender_->WaitForEnd();
    }

    // Даем команду остановиться текущему потоку.
    Stop();

    //
    // Асинхронно отправляем серверу команду проверить список
    // подключенных клиентов (таким образом мы сообщаем, что
    // клиент отключился и необходимо удалить из списка подключенных
    // клиентов "мертвых").
    //
    std::async(std::launch::async, on_client_disconnected_, GetID());

    DLOG(INFO) << "Client thread stopped";
}

void Client::OnStop()
{
    // Закрываем соединение.
    socket_->Disconnect();
}

void Client::ReadPointerEvent(const proto::PointerEvent &msg)
{
    //
    // Если класс выполнения команд ввода инициализирован.
    // Должен быть инициализирован, если сессия имеет возможность
    // управления рабочим столом, в обратном случае - не инициализирован.
    //
    if (input_handler_)
    {
        // Выполняем команду перемещения курсора и/или нажатия кнопок мыши.
        input_handler_->HandlePointer(msg);
    }
    else
    {
        LOG(ERROR) << "Client has sent a message that he was not allowed to send:"
                   << " PointerEvent. Message ignored.";
    }
}

void Client::ReadKeyEvent(const proto::KeyEvent &msg)
{
    //
    // Если класс выполнения команд ввода инициализирован.
    // Должен быть инициализирован, если сессия имеет возможность
    // управления рабочим столом, в обратном случае - не инициализирован.
    //
    if (input_handler_)
    {
        // Выполняем команду нажатия клавиши.
        input_handler_->HandleKeyboard(msg);
    }
    else
    {
        LOG(ERROR) << "Client has sent a message that he was not allowed to send:"
                   << " KeyEvent. Message ignored.";
    }
}

void Client::ReadVideoControl(const proto::VideoControl &msg)
{
    //
    // Если сессия имеет возможности управления рабочим столом или
    // просмотра рабочего стола.
    //
    if ((feature_mask_ & proto::FEATURE_DESKTOP_MANAGE) ||
        (feature_mask_ & proto::FEATURE_DESKTOP_VIEW))
    {
        // Если получена команда отключить передачу видео-пакетов.
        if (!msg.enable())
        {
            // Если отправка видео-пакетов была инициализирована.
            if (screen_sender_)
            {
                // Даем команду остановить поток отправки пакетов.
                screen_sender_->Stop();

                // Дожидаемся завершения потока.
                screen_sender_->WaitForEnd();

                // Уничтожаем экземпляр класса.
                screen_sender_.reset();
            }

            // Выходим, других действий не требуется.
            return;
        }

        // По умолчанию инициализируем формат пикселей клиента в RGB565.
        PixelFormat format = PixelFormat::MakeRGB565();

        // Если был получен формат пикселей от клиента.
        if (msg.has_pixel_format())
        {
            // Получаем его и переводим в локальный формат.
            const proto::VideoPixelFormat &pf = msg.pixel_format();

            format.set_bits_per_pixel(pf.bits_per_pixel());

            format.set_red_max(pf.red_max());
            format.set_green_max(pf.green_max());
            format.set_blue_max(pf.blue_max());

            format.set_red_shift(pf.red_shift());
            format.set_green_shift(pf.green_shift());
            format.set_blue_shift(pf.blue_shift());
        }

        // Если отправка видео-пакетов уже инициализирована.
        if (screen_sender_)
        {
            //
            // Перенастраиваем ее в соответствии с полученными от
            // клиента параметрами.
            //
            screen_sender_->Configure(msg.encoding(), format);
        }
        else
        {
            ScreenSender::OnMessageAvailabeCallback on_message_available =
                std::bind(&Client::WriteMessage, this, std::placeholders::_1);

            //
            // Инициализируем отправку видео-пакетов с полученными
            // от клиента параметрами.
            //
            screen_sender_.reset(new ScreenSender(msg.encoding(),
                                                  format,
                                                  on_message_available));

            // Запускаем поток отправки видео-пакетов.
            screen_sender_->Start();
        }
    }
    else
    {
        LOG(ERROR) << "Client has sent a message that he was not allowed to send:"
                   << " VideoControl. Message ignored.";
    }
}
