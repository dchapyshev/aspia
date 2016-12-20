/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "host/screen_sender.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

ScreenSender::ScreenSender(int32_t encoding,
                           const PixelFormat &client_pixel_format,
                           OnMessageCallback on_message) :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    client_pixel_format_(client_pixel_format),
    on_message_(on_message)
{
    Configure(encoding, client_pixel_format);

    SetThreadPriority(Priority::Highest);
    Start();
}

ScreenSender::~ScreenSender()
{
    if (!IsThreadTerminated())
    {
        Stop();
        WaitForEnd();
    }
}

void ScreenSender::Configure(int32_t encoding, const PixelFormat &client_pixel_format)
{
    // Блокируем отправку видео-пакетов.
    LockGuard<Lock> guard(&update_lock_);

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
        else
        {
            LOG(ERROR) << "Unsupported video encoding: " << current_encoding_;
            throw Exception("Unsupported video encoding.");
        }
    }

    // Сохраняем формат пикселей клиента.
    client_pixel_format_ = client_pixel_format;

    // Энкодер должен быть инициализирован.
    DCHECK(encoder_);
}

void ScreenSender::Worker()
{
    // Создаем сообщение для отправки клиенту.
    std::unique_ptr<proto::HostToClient> message(new proto::HostToClient());

    DesktopSize screen_size;
    DesktopSize prev_screen_size;

    PixelFormat host_pixel_format;
    PixelFormat prev_host_pixel_format;

    PixelFormat prev_client_pixel_format;

    DesktopRegion changed_region;

    try
    {
        // Создаем экземпляры классов захвата экрана и планировщика захвата экрана.
        std::unique_ptr<CaptureScheduler> scheduler(new CaptureScheduler());
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // Продолжаем цикл пока не будет дана команда завершить поток.
        while (!IsThreadTerminating())
        {
            // Делаем отметку времени начала операции.
            scheduler->BeginCapture();

            // Очищаем регион от предыдущих изменений.
            changed_region.Clear();

            //
            // Делаем захват изображения, получаем его размер, формат пикселей
            // и измененный регион.
            //
            const uint8_t *screen_buffer =
                capturer->CaptureImage(changed_region, screen_size, host_pixel_format);

            // Если измененный регион не пуст.
            if (!changed_region.IsEmpty())
            {
                //
                // Ставим блокировку отправки видео-пакетов (кодировка, формат пикселей
                // не могут изменяться при отправки одного логического обновления).
                //
                LockGuard<Lock> guard(&update_lock_);

                // Если изменились размера экрана или формат пикселей.
                if (screen_size          != prev_screen_size ||
                    client_pixel_format_ != prev_client_pixel_format ||
                    host_pixel_format    != prev_host_pixel_format)
                {
                    // Изменяем размер экнодера.
                    encoder_->Resize(screen_size, host_pixel_format, client_pixel_format_);

                    // Сохраняем новые значения.
                    prev_host_pixel_format   = host_pixel_format;
                    prev_client_pixel_format = client_pixel_format_;
                    prev_screen_size         = screen_size;
                }

                int32_t flags;

                //
                // Одно обновление экрана может быть разбито на несколько видео-пакетов.
                // Например, при использовании кодировки VIDEO_ENCODING_ZLIB каждый
                // измененный прямоугольник экрана отправляется отдельным пакетом.
                // Продолжаем цикл кодирования пока не будет отправлен последний пакет.
                //
                do
                {
                    // Очищаем сообщение от предыдущих данных.
                    message->Clear();

                    // Выполняем кодирование изображения.
                    flags = encoder_->Encode(message->mutable_video_packet(),
                                             screen_buffer,
                                             changed_region);

                    // Отправляем видео-пакет клиенту.
                    on_message_(message.get());

                } while (!(flags & proto::VideoPacket::LAST_PACKET));
            }

            scheduler->Sleep();
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }
}

void ScreenSender::OnStop()
{
    // Nothing
}

} // namespace aspia
