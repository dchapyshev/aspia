//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_zlib.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_zlib.h"

#include "base/logging.h"

namespace aspia {

VideoDecoderZLIB::VideoDecoderZLIB()
{
    // Nothing
}

bool VideoDecoderZLIB::Decode(const proto::VideoPacket& packet, DesktopFrame* frame)
{
    const uint8_t* src = reinterpret_cast<const uint8_t*>(packet.data().data());
    const int src_size = packet.data().size();
    int used = 0;

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        const proto::VideoRect& rect = packet.dirty_rect(i);

        uint8_t* dst = frame->GetFrameDataAtPos(rect.x(), rect.y());
        const int row_size = rect.width() * frame->Format().BytesPerPixel();

        // Consume all the data in the message.
        bool decompress_again = true;

        int row_y = 0;   // Текущая строка
        int row_pos = 0; // Положение в текущей строке

        while (decompress_again && used < src_size)
        {
            // Если мы достигли конца текущего прямоугольника, то переходи к следующему.
            if (row_y > rect.height() - 1)
                break;

            int written = 0;  // Количество байт записанный в буфер назначения
            int consumed = 0; // Количество байт, которые были взяты из исходного буфера

            // Распаковываем очередную порцию данных
            decompress_again = decompressor_.Process(src + used,
                                                     src_size - used,
                                                     dst + row_pos,
                                                     row_size - row_pos,
                                                     &consumed,
                                                     &written);
            used += consumed;
            row_pos += written;

            // Если мы полностью распаковали строку в прямоугольнике
            if (row_pos == row_size)
            {
                // Увеличиваем счетчик строк
                ++row_y;

                // Сбрасываем текущее положение в строке
                row_pos = 0;

                // Переходим к следующей строке в буфере назначения
                dst += frame->Stride();
            }
        }
    }

    // Сбрасываем декомпрессор после распаковки.
    decompressor_.Reset();

    return true;
}

} // namespace aspia
