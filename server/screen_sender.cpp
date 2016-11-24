/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "server/screen_sender.h"

#include "base/logging.h"

ScreenSender::ScreenSender(int32_t encoding,
                           const PixelFormat &client_pixel_format,
                           OnMessageAvailabeCallback on_message_available) :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    client_pixel_format_(client_pixel_format),
    on_message_available_(on_message_available)
{
    Configure(encoding, client_pixel_format);
}

ScreenSender::~ScreenSender()
{
    // Nothing
}

void ScreenSender::Configure(int32_t encoding, const PixelFormat &client_pixel_format)
{
    // Блокируем отправку видео-пакетов.
    LockGuard<Mutex> guard(&update_lock_);

    // Если изменилась кодировка.
    if (current_encoding_ != encoding)
    {
        // Переинициализируем кодировщик.
        if (encoding == proto::VIDEO_ENCODING_VP8)
        {
            encoder_.reset(new VideoEncoderVP8());
            current_encoding_ = encoding;
        }
        else if (encoding == proto::VIDEO_ENCODING_ZLIB)
        {
            encoder_.reset(new VideoEncoderZLIB());
            current_encoding_ = encoding;
        }
        else if (encoding == proto::VIDEO_ENCODING_RAW)
        {
            encoder_.reset(new VideoEncoderRAW());
            current_encoding_ = encoding;
        }
        else
        {
            LOG(WARNING) << "Unsupported video encoding: " << current_encoding_;
            throw Exception("Unsupported video encoding.");
        }
    }

    // Сохраняем формат пикселей клиента.
    client_pixel_format_ = client_pixel_format;

    // Энкодер должен быть инициализирован.
    CHECK(encoder_);
}

void ScreenSender::Worker()
{
    DLOG(INFO) << "Screen sender thread started";

    //
    // Для минимизации задержек при отправке сообщений видео-пакетов
    // устанавливаем высокий приоритет для потока.
    //
    SetThreadPriority(Priority::Highest);

    DesktopRegion changed_region;

    DesktopSize screen_size;
    DesktopSize prev_screen_size;

    PixelFormat host_pixel_format;
    PixelFormat prev_host_pixel_format;

    PixelFormat prev_client_pixel_format;

    try
    {
        // Создаем экземпляры классов захвата экрана и планировщика захвата экрана.
        std::unique_ptr<CaptureScheduler> scheduler(new CaptureScheduler());
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // Создаем сообщение для отправки клиенту.
        std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

        // Продолжаем цикл пока не будет дана команда завершить поток.
        while (!IsEndOfThread())
        {
            // Делаем отметку времени начала операции
            scheduler->BeginCapture();

            //
            // Делаем захват изображения, получаем его размер, формат пикселей
            // и измененный регион.
            //
            const uint8_t *screen_buffer =
                capturer->CaptureImage(changed_region, screen_size, host_pixel_format);

            // Если измененный регион не пуст.
            if (!changed_region.is_empty())
            {
                //
                // Ставим блокировку отправки видео-пакетов (кодировка, формат пикселей
                // не могут изменяться при отправки одного логического обновления).
                //
                LockGuard<Mutex> guard(&update_lock_);

                // Если изменились размера экрана или формат пикселей.
                if (screen_size != prev_screen_size ||
                    client_pixel_format_ != prev_client_pixel_format ||
                    host_pixel_format != prev_host_pixel_format)
                {
                    // Изменяем размер экнодера.
                    encoder_->Resize(screen_size, host_pixel_format, client_pixel_format_);

                    // Сохраняем новые значения.
                    prev_host_pixel_format = host_pixel_format;
                    prev_client_pixel_format = client_pixel_format_;
                    prev_screen_size = screen_size;
                }

                VideoEncoder::Status status = VideoEncoder::Status::Next;

                //
                // Одно обновление экрана может быть разбито на несколько видео-пакетов.
                // Например, при использовании кодировки VIDEO_ENCODING_ZLIB каждый
                // измененный прямоугольник экрана отправляется отдельным пакетом.
                // Продолжаем цикл кодирования пока не будет отправлен последний пакет.
                //
                while (status == VideoEncoder::Status::Next)
                {
                    // Очищаем сообщение от предыдущих данных.
                    message->Clear();

                    // Выполняем кодирование изображения.
                    status = encoder_->Encode(message->mutable_video_packet(),
                                              screen_buffer,
                                              changed_region);

                    // Отправляем видео-пакет клиенту.
                    on_message_available_(message.get());
                }
            }

            // Ждем следующей отправки.
            Sleep(scheduler->NextCaptureDelay());
        }
    }
    catch (const Exception &err)
    {
        //
        // Если произошло исключение при отправке видео-пакета или другая ошибка
        // (например, при инициализации каких-либо классов), то завершаем поток.
        //
        LOG(ERROR) << "Exception in screen sender thread: " << err.What();
    }

    DLOG(INFO) << "Screen sender thread stopped";
}

void ScreenSender::OnStop()
{
    // Nothing
}
