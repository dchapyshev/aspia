/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_reciever.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "video_test/screen_reciever.h"

ScreenReciever::ScreenReciever() :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN)
{

}

ScreenReciever::~ScreenReciever()
{

}

void ScreenReciever::ReadPacket(proto::VideoPacket *packet)
{
    LockGuard<Mutex> guard(&packet_queue_lock_);

    // Добавляем пакет в очередь
    packet_queue_.push(packet->SerializeAsString());

    // Уведомляем поток обаботки пакетов о наличии новых пакетов
    packet_event_.Notify();
}

void ScreenReciever::Worker()
{
    // Если класс окна не инициализирован
    if (!window_)
    {
        // Инициализируем его и запускаем поток
        window_.reset(new ScreenWindowWin(nullptr));
        window_->Start();
    }

    PixelFormat pixel_format = PixelFormat::MakeARGB();

    proto::VideoPacket *packet = google::protobuf::Arena::CreateMessage<proto::VideoPacket>(&arena_);

    // Продолжаем цикл пока поток окна на завершится
    while (!window_->IsEndOfThread())
    {
        // Ждем уведомления о том, что доступен новый пакет
        packet_event_.WaitForEvent();

        // Продолжаем обрабатывать очередь пока она не будет обработана полностью
        while (packet_queue_.size())
        {
            // Блокируем очередь пакетов
            packet_queue_lock_.lock();

            // Получаем первый пакет из очереди
            packet->ParseFromString(packet_queue_.front());

            // Удаляем первый пакет из очереди
            packet_queue_.pop();

            // Разблокируем очередь пакетов
            packet_queue_lock_.unlock();

            // Если пакет является первым в логическом обновлении
            if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
            {
                const proto::VideoPacketFormat &packet_format = packet->format();

                // Получаем кодировку
                proto::VideoEncoding encoding = packet_format.encoding();

                // Переинициализируем декодер при необходимости
                if (current_encoding_ != encoding)
                {
                    current_encoding_ = encoding;

                    if (encoding == proto::VIDEO_ENCODING_RAW)
                    {
                        decoder_.reset(new VideoDecoderRAW());
                    }
                    else if (encoding == proto::VIDEO_ENCODING_VP8)
                    {
                        decoder_.reset(new VideoDecoderVP8());
                    }
                    else if (encoding == proto::VIDEO_ENCODING_ZLIB)
                    {
                        decoder_.reset(new VideoDecoderZLIB());
                    }
                    else
                    {
                        LOG(INFO) << "Unknown encoding: " << packet_format.encoding();
                    }
                }

                // Если имеется формат пикселей в сообщении
                if (packet_format.has_pixel_format())
                {
                    // Получаем его
                    const proto::VideoPixelFormat &video_format = packet_format.pixel_format();

                    pixel_format.set_bits_per_pixel(video_format.bits_per_pixel());

                    pixel_format.set_red_max(video_format.red_max());
                    pixel_format.set_green_max(video_format.green_max());
                    pixel_format.set_blue_max(video_format.blue_max());

                    pixel_format.set_red_shift(video_format.red_shift());
                    pixel_format.set_green_shift(video_format.green_shift());
                    pixel_format.set_blue_shift(video_format.blue_shift());
                }
                else
                {
                    // Или используем формат по-умолчанию
                    pixel_format = PixelFormat::MakeARGB();
                }

                // Отправляем окну команду изменить размер
                window_->Resize(DesktopSize(packet_format.screen_width(),
                                            packet_format.screen_height()),
                                pixel_format);
            }

            // Если декодер инициализирован
            if (decoder_)
            {
                // Получаем буфер, в который будем производить декодирование
                ScreenWindowWin::LockedMemory memory = window_->GetBuffer();

                // Если декодер успешно декодировал пакет
                if (decoder_->Decode(packet, pixel_format, memory.get()))
                {
                    // Сообщаем окну, что буфер изменился
                    window_->Invalidate();
                }
            }
        }
    }
}

void ScreenReciever::OnStart() {}

void ScreenReciever::OnStop() {}
