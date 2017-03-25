//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_decoder.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_decoder.h"

#include "base/logging.h"

namespace aspia {

CursorDecoder::CursorDecoder()
{
    // Nothing
}

bool CursorDecoder::DecompressCursor(const proto::CursorShape& cursor_shape, uint8_t* image)
{
    const uint8_t* src = reinterpret_cast<const uint8_t*>(cursor_shape.data().data());
    const int src_size = cursor_shape.data().size();
    const int row_size = cursor_shape.width() * sizeof(uint32_t);

    // Consume all the data in the message.
    bool decompress_again = true;
    int used = 0;

    int row_y = 0;   // Текущая строка.
    int row_pos = 0; // Положение в текущей строке.

    while (decompress_again && used < src_size)
    {
        if (row_y > cursor_shape.height() - 1)
        {
            LOG(ERROR) << "Too much data is received for the given rectangle";
            return false;
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

    return true;
}

const MouseCursor* CursorDecoder::Decode(const proto::CursorShape& cursor_shape)
{
    int cache_index = -1;

    if (cursor_shape.flags() & proto::CursorShape::CACHE)
    {
        // Биты 0-4 содержат позицию курсора в индексе.
        cache_index = cursor_shape.flags() & 0x1F;
    }
    else
    {
        int width = cursor_shape.width();
        int height = cursor_shape.height();

        if (width  <= 0 || width  > (SHRT_MAX / 2) ||
            height <= 0 || height > (SHRT_MAX / 2))
        {
            DLOG(ERROR) << "Cursor dimensions are out of bounds for SetCursor: "
                        << width << "x" << height;
            return nullptr;
        }

        size_t image_size = width * height * sizeof(uint32_t);
        std::unique_ptr<uint8_t[]> image(new uint8_t[image_size]);

        if (!DecompressCursor(cursor_shape, image.get()))
            return nullptr;

        std::unique_ptr<MouseCursor> mouse_cursor(new MouseCursor(std::move(image),
                                                                  width,
                                                                  height,
                                                                  cursor_shape.hotspot_x(),
                                                                  cursor_shape.hotspot_y()));

        if (cursor_shape.flags() & proto::CursorShape::RESET_CACHE)
        {
            size_t cache_size = cursor_shape.flags() & 0x1F;

            if (!MouseCursorCache::IsValidCacheSize(cache_size))
                return nullptr;

            cache_.reset(new MouseCursorCache(cache_size));
        }

        if (!cache_)
        {
            DLOG(ERROR) << "Host did not send cache reset command";
            return nullptr;
        }

        cache_index = cache_->Add(std::move(mouse_cursor));
    }

    return cache_->Get(cache_index);
}

} // namespace aspia
