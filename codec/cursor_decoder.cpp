//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_decoder.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_decoder.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

CursorDecoder::CursorDecoder()
{
    // Nothing
}

CursorDecoder::~CursorDecoder()
{
    // Nothing
}

void CursorDecoder::DecompressCursor(const proto::CursorShape &msg, uint8_t *image)
{
    const uint8_t *src = reinterpret_cast<const uint8_t*>(msg.data().data());
    const int src_size = msg.data().size();
    const int row_size = msg.width() * sizeof(uint32_t);

    // Consume all the data in the message.
    bool decompress_again = true;
    int used = 0;

    int row_y = 0;   // Текущая строка.
    int row_pos = 0; // Положение в текущей строке.

    while (decompress_again && used < src_size)
    {
        if (row_y > msg.height() - 1)
        {
            LOG(ERROR) << "Too much data is received for the given rectangle";
            throw Exception("Too much data is received for the given rectangle");
        }

        int written = 0;  // Количество байт записанный в буфер назначения.
        int consumed = 0; // Количество байт, которые были взяты из исходного буфера.

        // Распаковываем очередную порцию данных.
        decompress_again = decompressor_.Process(src + used,
                                                 src_size - used,
                                                 image + row_pos,
                                                 row_size - row_pos,
                                                 &consumed,
                                                 &written);
        used += consumed;
        row_pos += written;

        // Если мы полностью распаковали строку в прямоугольнике.
        if (row_pos == row_size)
        {
            // Увеличиваем счетчик строк.
            ++row_y;

            // Сбрасываем текущее положение в строке.
            row_pos = 0;

            // Переходим к следующей строке в буфере назначения.
            image += row_size;
        }
    }

    // Сбрасываем декомпрессор после распаковки каждого прямоугольника.
    decompressor_.Reset();
}

const MouseCursor* CursorDecoder::Decode(const proto::CursorShape &msg)
{
    int cache_index = -1;

    switch (msg.encoding())
    {
        case proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_CACHE:
        {
            cache_index = msg.cache_index();
        }
        break;

        case proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_ZLIB:
        {
            std::unique_ptr<MouseCursor> mouse_cursor(new MouseCursor());

            mouse_cursor->SetSize(msg.width(), msg.height());
            mouse_cursor->SetHotspot(msg.hotspot_x(), msg.hotspot_y());

            size_t image_size = msg.width() * msg.height() * sizeof(uint32_t);
            std::unique_ptr<uint8_t[]> image(new uint8_t[image_size]);

            DecompressCursor(msg, image.get());

            mouse_cursor->SetData(std::move(image), image_size);

            // Не нулевое значение индекса означает команду сброса кеша.
            if (msg.cache_index() != 0)
            {
                // Очищаем кеш.
                cache_.Clear();
            }

            cache_index = cache_.Add(std::move(mouse_cursor));
        }
        break;

        default:
        {
            LOG(ERROR) << "Unsupported cursor encoding: " << msg.encoding();
            throw Exception("Unsupported cursor encoding");
        }
        break;
    }

    return cache_.Get(cache_index);
}

} // namespace aspia
