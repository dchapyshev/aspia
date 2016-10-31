/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "video_test/screen_sender.h"

ScreenSender::ScreenSender(ScreenReciever *reciever) : reciever_(reciever) {}

ScreenSender::~ScreenSender() {}

void ScreenSender::Worker()
{
    // Инициализируем классы планировщика захвата экрана, самого класса захвата экрана и кодировщика
    scheduler_.reset(new CaptureScheduler());
    capturer_.reset(new CapturerGDI());
    encoder_.reset(new VideoEncoderZLIB());

    // Запускаем поток получения видео-пакетов
    reciever_->Start();

    DesktopRegion region;
    DesktopSize size;
    PixelFormat pixel_format;

    // Продолжаем цикл пока не завершится поток получателя сообщений
    while (!reciever_->IsEndOfThread())
    {
        // Делаем отметку времени начала операции
        scheduler_->BeginCapture();

        // Делаем захват изображения, получаем его размер, формат пикселей и измененный регион
        const uint8_t *buffer = capturer_->CaptureImage(region, size, pixel_format);

        // Если измененный регион не пуст
        if (!region.is_empty())
        {
            int last = 0;

            // Продолжаем цикл кодирования пока не будет отправлен последний пакет
            while (!last)
            {
                // Кодируем кадр
                proto::VideoPacket *packet =
                    encoder_->Encode(size, pixel_format, PixelFormat::MakeRGB64(), region, buffer);

                // Если не удалось создать пакет, то прерываем цикл
                if (!packet)
                {
                    break;
                }

                // Если отправлен последний пакет, то прерываем цикл
                last = (packet->flags() & proto::VideoPacket::LAST_PACKET);

                // Отправляем указатель на пакет получателю
                reciever_->ReadPacket(packet);
            }
        }

        // Ждем следующего захвата экрана
        Sleep(scheduler_->NextCaptureDelay());
    }
}

void ScreenSender::OnStart() {}

void ScreenSender::OnStop() {}
