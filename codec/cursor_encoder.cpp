//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_encoder.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_encoder.h"

#include "base/logging.h"

namespace aspia {

CursorEncoder::CursorEncoder() :
    compressor_(6)
{
    // Nothing
}

CursorEncoder::~CursorEncoder()
{
    // Nothing
}

uint8_t* CursorEncoder::GetOutputBuffer(proto::CursorShape *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void CursorEncoder::CompressCursor(proto::CursorShape *packet, const MouseCursor *mouse_cursor)
{
    // Делаем сброс компрессора при сжатии каждого курсора.
    compressor_.Reset();

    int width = mouse_cursor->Size().Width();
    int height = mouse_cursor->Size().Height();

    // Размер строки курсора в байтах.
    const int row_size = width * sizeof(uint32_t);

    int packet_size = row_size * height;
    packet_size += packet_size / 100 + 16;

    uint8_t *compressed_pos = GetOutputBuffer(packet, packet_size);
    uint8_t *source_pos = mouse_cursor->Data();

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
            packet->mutable_data()->resize(filled);
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

void CursorEncoder::Encode(proto::CursorShape *packet, std::unique_ptr<MouseCursor> mouse_cursor)
{
    int index = cache_.Find(mouse_cursor.get());

    // Курсор не найден в кеше.
    if (index == -1)
    {
        packet->set_encoding(proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_ZLIB);

        packet->set_width(mouse_cursor->Size().Width());
        packet->set_height(mouse_cursor->Size().Height());
        packet->set_hotspot_x(mouse_cursor->Hotspot().x());
        packet->set_hotspot_y(mouse_cursor->Hotspot().y());

        CompressCursor(packet, mouse_cursor.get());

        // Если кеш пуст.
        if (cache_.IsEmpty())
        {
            //
            // Устанавливаем индекс в кеше в любое не нулевое значение для того,
            // чтобы на стороне клиента кеш был сброшен.
            //
            packet->set_cache_index(-1);
        }

        // Добавляем курсор в кеш.
        cache_.Add(std::move(mouse_cursor));
    }
    else
    {
        packet->set_encoding(proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_CACHE);
        packet->set_cache_index(index);
    }
}

} // namespace aspia
