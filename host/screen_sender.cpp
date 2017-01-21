//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_sender.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_sender.h"

#include <thread>

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

ScreenSender::ScreenSender(OnMessageCallback on_message_callback) :
    encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    on_message_callback_(on_message_callback)
{
    // Nothing
}

ScreenSender::~ScreenSender()
{
    if (IsActiveThread())
    {
        Stop();
        WaitForEnd();
    }
}

void ScreenSender::Configure(const proto::VideoControl &msg)
{
    // Блокируем отправку видео-пакетов.
    LockGuard<Lock> guard(&update_lock_);

    // Если изменилась кодировка.
    if (encoding_ != msg.encoding())
    {
        encoding_ = msg.encoding();

        // Переинициализируем кодировщик.
        switch (encoding_)
        {
            case proto::VIDEO_ENCODING_VP8:
                encoder_.reset(new VideoEncoderVP8());
                break;

            case proto::VIDEO_ENCODING_VP9:
                encoder_.reset(new VideoEncoderVP9());
                break;

            case proto::VIDEO_ENCODING_ZLIB:
                encoder_.reset(new VideoEncoderZLIB());
                break;

            default:
                LOG(ERROR) << "Unsupported video encoding: " << encoding_;
                throw Exception("Unsupported video encoding");
                break;
        }

        //
        // Сбрасываем размер экрана, чтобы кодировщик был сконфигурирован в соответствии
        // с текущим размером.
        //
        prev_size_.Clear();
    }

    if (encoding_ == proto::VIDEO_ENCODING_ZLIB)
    {
        VideoEncoderZLIB *encoder = reinterpret_cast<VideoEncoderZLIB*>(encoder_.get());

        // Если получено новое значение уровня сжатия.
        if (msg.compress_ratio())
        {
            // Устанавливаем новый уровень сжатия.
            encoder->SetCompressRatio(msg.compress_ratio());
        }
    }

    // Если был получен формат пикселей от клиента.
    if (msg.has_pixel_format())
    {
        curr_format_.FromVideoPixelFormat(msg.pixel_format());
    }

    // Энкодер должен быть инициализирован.
    DCHECK(encoder_);

    // Если получена команда отключить эффекты рабочего стола.
    if (msg.flags() & proto::VideoControl::DISABLE_DESKTOP_EFFECTS)
    {
        // Если они еще вы выключены.
        if (!desktop_effects_)
        {
            // Выключаем.
            desktop_effects_.reset(new ScopedDesktopEffects());
        }
    }
    else
    {
        // Включаем эффекты рабочего стола.
        desktop_effects_.reset();
    }

    int32_t interval = msg.update_interval();

    // Если получено новое значение интервала обновления и оно не равно предыдущему.
    if (interval)
    {
        // Переинициализируем планировщик.
        scheduler_.reset(new CaptureScheduler(interval));
    }

    // Если планировщик не инициализирован.
    if (!scheduler_)
    {
        // Инциализируем его с интервалом обновления по умолчанию.
        scheduler_.reset(new CaptureScheduler(30));
    }

    // Если поток не запущен на выполнение.
    if (!IsActiveThread())
    {
        // Устанавливаем приоритет для потока и запускаем его.
        SetThreadPriority(Priority::Highest);
        Start();
    }
}

void ScreenSender::Worker()
{
    // Создаем сообщение для отправки клиенту.
    std::unique_ptr<proto::HostToClient> message(new proto::HostToClient());

    try
    {
        // Создаем экземпляр класса захвата экрана.
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // Продолжаем цикл пока не будет дана команда завершить поток.
        while (!IsThreadTerminating())
        {
            // Делаем отметку времени начала операции.
            scheduler_->BeginCapture();

            //
            // Делаем захват изображения, получаем его размер, формат пикселей
            // и измененный регион.
            //
            const uint8_t *screen_buffer = capturer->CaptureImage(&dirty_region_, &curr_size_);

            // Если измененный регион не пуст.
            if (!dirty_region_.IsEmpty())
            {
                //
                // Ставим блокировку отправки видео-пакетов (кодировка, формат пикселей
                // не могут изменяться при отправки одного логического обновления).
                //
                LockGuard<Lock> guard(&update_lock_);

                // Если изменились размера экрана или формат пикселей.
                if (curr_size_ != prev_size_ || curr_format_ != prev_format_)
                {
                    // Сохраняем новые значения.
                    prev_format_ = curr_format_;
                    prev_size_ = curr_size_;

                    // Изменяем размер экнодера.
                    encoder_->Resize(curr_size_, curr_format_);

                    // После изменений добавляем в изменный регион всю область экрана.
                    dirty_region_.AddRect(DesktopRect::MakeSize(curr_size_));
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
                                             dirty_region_);

                    // Отправляем видео-пакет клиенту.
                    on_message_callback_(message.get());

                } while (!(flags & proto::VideoPacket::LAST_PACKET));

                // Очищаем регион от предыдущих изменений.
                dirty_region_.Clear();
            }

            scheduler_->Wait();
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
