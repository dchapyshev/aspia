//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_sender.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_sender.h"
#include "base/logging.h"

namespace aspia {

static const uint32_t kDefaultUpdateInterval = 30;
static const uint32_t kMinUpdateInterval = 15;
static const uint32_t kMaxUpdateInterval = 100;

ScreenSender::ScreenSender()
{
    // Nothing
}

ScreenSender::~ScreenSender()
{
    StopSending();
}

bool ScreenSender::StartSending(const proto::DesktopConfig& config,
                                const MessageCallback& message_callback)
{
    // Блокируем обновление.
    AutoLock lock(update_lock_);

    message_callback_ = message_callback;

    if (config.update_interval() < kMinUpdateInterval ||
        config.update_interval() > kMaxUpdateInterval)
    {
        LOG(ERROR) << "Wrong update interval: " << config.update_interval();
        return false;
    }

    // Если кодировщик не инициализирован или изменилась кодировка.
    if (!video_encoder_ || config_.encoding() != config.encoding())
    {
        // Инициализируем кодировщик.
        switch (config.encoding())
        {
            case proto::VIDEO_ENCODING_VP8:
                video_encoder_.reset(new VideoEncoderVP8());
                break;

            case proto::VIDEO_ENCODING_VP9:
                video_encoder_.reset(new VideoEncoderVP9());
                break;

            case proto::VIDEO_ENCODING_ZLIB:
                video_encoder_.reset(new VideoEncoderZLIB());
                break;

            default:
                LOG(ERROR) << "Unsupported video encoding: " << config.encoding();
                return false;
        }
    }

    if (config.encoding() == proto::VIDEO_ENCODING_ZLIB)
    {
        VideoEncoderZLIB* zlib_video_encoder =
            reinterpret_cast<VideoEncoderZLIB*>(video_encoder_.get());

        // Если получено новое значение уровня сжатия.
        if (config.compress_ratio())
        {
            // Устанавливаем новый уровень сжатия.
            if (!zlib_video_encoder->SetCompressRatio(config.compress_ratio()))
                return false;
        }

        // Если был получен формат пикселей от клиента.
        if (config.has_pixel_format())
        {
            PixelFormat format(config.pixel_format());
            zlib_video_encoder->SetPixelFormat(format);
        }
    }

    // Если получена команда включить отправку изображения курсора.
    if (config.flags() & proto::DesktopConfig::ENABLE_CURSOR_SHAPE)
    {
        // Если энкодер изображения курсора не инициализирован.
        if (!cursor_encoder_)
            cursor_encoder_.reset(new CursorEncoder());
    }
    else
    {
        // Деинициализируем энкодер (отключаем отправку).
        cursor_encoder_.reset();
    }

    // Если получена команда отключить эффекты рабочего стола.
    if (!(config.flags() & proto::DesktopConfig::ENABLE_DESKTOP_EFFECTS))
    {
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

    // Сохраняем копию конфигурации.
    config_.CopyFrom(config);

    // Если поток не запущен на выполнение.
    if (!IsActive())
        Start(Priority::AboveNormal);

    return true;
}

void ScreenSender::StopSending()
{
    Stop();
    Wait();
}

void ScreenSender::Worker()
{
    // Создаем экземпляр класса захвата экрана.
    capturer_.reset(new CapturerGDI());

    // Продолжаем цикл пока не будет дана команда завершить поток.
    while (!IsTerminating())
    {
        CaptureScheduler scheduler;

        {
            //
            // Ставим блокировку отправки видео-пакетов (кодировка, формат пикселей
            // не могут изменяться при отправки одного логического обновления).
            //
            AutoLock lock(update_lock_);

            bool desktop_change;

            //
            // Делаем захват изображения, получаем его размер, формат пикселей
            // и измененный регион.
            //
            const DesktopFrame* frame = capturer_->CaptureImage(&desktop_change);

            if (desktop_change)
            {
                if (!(config_.flags() & proto::DesktopConfig::ENABLE_DESKTOP_EFFECTS))
                {
                    // Выключаем.
                    desktop_effects_.reset(new ScopedDesktopEffects());
                }
                else
                {
                    // Включаем эффекты рабочего стола.
                    desktop_effects_.reset();
                }
            }

            // Если энкодер курсора инициализирован (включена отправка изображения курсора).
            if (cursor_encoder_)
            {
                // Выполняем захват изображения курсора.
                std::unique_ptr<MouseCursor> mouse_cursor(capturer_->CaptureCursor());

                // Если курсор получен (отличается от предыдущего).
                if (mouse_cursor)
                {
                    // Кодируем изображение курсора.
                    cursor_encoder_->Encode(message_.mutable_cursor_shape(),
                                            std::move(mouse_cursor));
                }
            }

            // Если измененный регион не пуст.
            if (!frame->UpdatedRegion().IsEmpty())
            {
                // Выполняем кодирование изображения.
                video_encoder_->Encode(message_.mutable_video_packet(), frame);

                // Отправляем видео-пакет клиенту.
                if (!message_callback_(message_))
                    return;

                // Очищаем сообщение от предыдущих данных.
                message_.Clear();
            }
            // Если изображение экрана не имеет отличий, но имеются изменения курсора.
            else if (message_.has_cursor_shape())
            {
                // Отправляем сообщение.
                if (!message_callback_(message_))
                    return;

                // Очищаем сообщение от предыдущих данных.
                message_.Clear();
            }
        }

        // Ждем следующего обновления экрана и курсора.
        Sleep(scheduler.NextCaptureDelay(config_.update_interval()));
    }
}

void ScreenSender::OnStop()
{
    // Nothing
}

} // namespace aspia
