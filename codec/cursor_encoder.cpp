//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_encoder.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_encoder.h"

#include "base/logging.h"

namespace aspia {

// Размер кеша может быть в интервале от 2 до 31.
static const uint8_t kCacheSize = 8;

// Степень сжатия может быть в интервале от 1 до 9.
static const int32_t kCompressionRatio = 6;

CursorEncoder::CursorEncoder() :
    compressor_(kCompressionRatio),
    cache_(kCacheSize)
{
    static_assert(kCacheSize >= 2 && kCacheSize <= 31, "Invalid cache size");
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 9, "Invalid compression ratio");
}

uint8_t* CursorEncoder::GetOutputBuffer(proto::CursorShape* cursor_shape, size_t size)
{
    cursor_shape->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(cursor_shape->mutable_data()->data()));
}

void CursorEncoder::CompressCursor(proto::CursorShape* cursor_shape, const MouseCursor* mouse_cursor)
{
    // Делаем сброс компрессора при сжатии каждого курсора.
    compressor_.Reset();

    int width = mouse_cursor->Size().Width();
    int height = mouse_cursor->Size().Height();

    // Размер строки курсора в байтах.
    const int row_size = width * sizeof(uint32_t);

    int packet_size = row_size * height;
    packet_size += packet_size / 100 + 16;

    uint8_t* compressed_pos = GetOutputBuffer(cursor_shape, packet_size);
    const uint8_t* source_pos = mouse_cursor->Data();

    int filled = 0;   // Количество байт в буфере назначения.
    int row_pos = 0;  // Position in the current row in bytes.
    int row_y = 0;    // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // Если мы достигли последней строки в прямоугольнике
        if (row_y == height - 1)
        {
            // Ставим соответствующий флаг
            flush = Compressor::CompressorFinish;
        }

        int consumed = 0; // Количество байт, которое было взято из исходного буфера.
        int written = 0;  // Количество байт, которое было записано в буфер назначения.

        // Сжимаем очередную порцию данных.
        compress_again = compressor_.Process(source_pos + row_pos, row_size - row_pos,
                                             compressed_pos + filled, packet_size - filled,
                                             flush, &consumed, &written);

        row_pos += consumed; // Сдвигаем положение с текущей строке прямоугольника.
        filled += written;   // Увеличиваем счетчик итогового размера буфера назначения.

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            cursor_shape->mutable_data()->resize(filled);
            return;
        }

        // Если мы достигли конца текущей строки в прямоугольнике и это не последняя строка.
        if (row_pos == row_size && row_y < height - 1)
        {
            // Обнуляаем положение в текущей строке.
            row_pos = 0;

            // Переходим к следующей строке в буфере.
            source_pos += row_size;

            // Увеличиваем номер текущей строки.
            ++row_y;
        }
    }
}

void CursorEncoder::Encode(proto::CursorShape* cursor_shape, std::unique_ptr<MouseCursor> mouse_cursor)
{
    int index = cache_.Find(mouse_cursor.get());

    // Курсор не найден в кеше.
    if (index == -1)
    {
        cursor_shape->set_width(mouse_cursor->Size().Width());
        cursor_shape->set_height(mouse_cursor->Size().Height());
        cursor_shape->set_hotspot_x(mouse_cursor->Hotspot().x());
        cursor_shape->set_hotspot_y(mouse_cursor->Hotspot().y());

        CompressCursor(cursor_shape, mouse_cursor.get());

        //
        // Если кеш пуст, то устанавливаем флаг сброса кеша на стороне клиента и передаем
        // максимальный размер кеша.
        //
        cursor_shape->set_flags(cache_.IsEmpty() ?
            (proto::CursorShape::RESET_CACHE | (kCacheSize & 0x1F)) : 0);

        // Добавляем курсор в кеш.
        cache_.Add(std::move(mouse_cursor));
    }
    else
    {
        cursor_shape->set_flags(proto::CursorShape::CACHE | (index & 0x1F));
    }
}

} // namespace aspia
