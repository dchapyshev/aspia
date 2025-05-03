//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/codec/pixel_translator.h"

#include "base/macros_magic.h"
#include "build/build_config.h"

#include <limits>

namespace base {

namespace {

const int kBlockSize = 16;

template<typename SourceT, typename TargetT>
class PixelTranslatorT final : public PixelTranslator
{
public:
    PixelTranslatorT(const PixelFormat& source_format,
                     const PixelFormat& target_format)
        : source_format_(source_format),
          target_format_(target_format)
    {
        red_table_ = std::make_unique<quint32[]>(source_format_.redMax() + 1);
        green_table_ = std::make_unique<quint32[]>(source_format_.greenMax() + 1);
        blue_table_ = std::make_unique<quint32[]>(source_format_.blueMax() + 1);

        for (quint32 i = 0; i <= source_format_.redMax(); ++i)
        {
            red_table_[i] = ((i * target_format_.redMax() + source_format_.redMax() / 2) /
                             source_format_.redMax()) << target_format_.redShift();
        }

        for (quint32 i = 0; i <= source_format_.greenMax(); ++i)
        {
            green_table_[i] = ((i * target_format_.greenMax() + source_format_.greenMax() / 2) /
                               source_format_.greenMax()) << target_format_.greenShift();
        }

        for (quint32 i = 0; i <= source_format_.blueMax(); ++i)
        {
            blue_table_[i] = ((i * target_format_.blueMax() + source_format_.blueMax() / 2) /
                              source_format_.blueMax()) << target_format_.blueShift();
        }
    }

    ~PixelTranslatorT() final = default;

    void translatePixel(const SourceT* src_ptr, TargetT* dst_ptr)
    {
        const quint32 red = red_table_[
            *src_ptr >> source_format_.redShift() & source_format_.redMax()];
        const quint32 green = green_table_[
            *src_ptr >> source_format_.greenShift() & source_format_.greenMax()];
        const quint32 blue = blue_table_[
            *src_ptr >> source_format_.blueShift() & source_format_.blueMax()];

        *dst_ptr = static_cast<TargetT>(red | green | blue | 0xFF000000);
    }

    void translate(const quint8* src, int src_stride,
                   quint8* dst, int dst_stride,
                   int width, int height) final
    {
        const int block_count = width / kBlockSize;
        const int partial_width = width - (block_count * kBlockSize);

        for (int y = 0; y < height; ++y)
        {
            const SourceT* src_ptr = reinterpret_cast<const SourceT*>(src);
            TargetT* dst_ptr = reinterpret_cast<TargetT*>(dst);

            for (int x = 0; x < block_count; ++x)
            {
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
                translatePixel(src_ptr++, dst_ptr++);
            }

            for (int x = 0; x < partial_width; ++x)
                translatePixel(src_ptr++, dst_ptr++);

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    std::unique_ptr<quint32[]> red_table_;
    std::unique_ptr<quint32[]> green_table_;
    std::unique_ptr<quint32[]> blue_table_;

    PixelFormat source_format_;
    PixelFormat target_format_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorT);
};

template<typename SourceT, typename TargetT>
class PixelTranslatorFrom8_16bppT final : public PixelTranslator
{
public:
    PixelTranslatorFrom8_16bppT(const PixelFormat& source_format,
                                const PixelFormat& target_format)
        : source_format_(source_format),
          target_format_(target_format)
    {
        static_assert(sizeof(SourceT) == sizeof(quint8) || sizeof(SourceT) == sizeof(quint16));

        const size_t table_size = std::numeric_limits<SourceT>::max() + 1;
        table_ = std::make_unique<quint32[]>(table_size);

        quint32 source_red_mask = source_format.redMax() << source_format.redShift();
        quint32 source_green_mask = source_format.greenMax() << source_format.greenShift();
        quint32 source_blue_mask = source_format.blueMax() << source_format.blueShift();

        for (quint32 i = 0; i < table_size; ++i)
        {
            quint32 source_red = (i & source_red_mask) >> source_format.redShift();
            quint32 source_green = (i & source_green_mask) >> source_format.greenShift();
            quint32 source_blue = (i & source_blue_mask) >> source_format.blueShift();

            quint32 target_red =
                (source_red * target_format.redMax() / source_format.redMax()) << target_format.redShift();
            quint32 target_green =
                (source_green * target_format.greenMax() / source_format.greenMax()) << target_format.greenShift();
            quint32 target_blue =
                (source_blue * target_format.blueMax() / source_format.blueMax()) << target_format.blueShift();

            table_[i] = target_red | target_green | target_blue | 0xFF000000;
        }
    }

    ~PixelTranslatorFrom8_16bppT() final = default;

    void translate(const quint8* src, int src_stride,
                   quint8* dst, int dst_stride,
                   int width, int height) final
    {
        const int block_count = width / kBlockSize;
        const int partial_width = width - (block_count * kBlockSize);

        for (int y = 0; y < height; ++y)
        {
            const SourceT* src_ptr = reinterpret_cast<const SourceT*>(src);
            TargetT* dst_ptr = reinterpret_cast<TargetT*>(dst);

            for (int x = 0; x < block_count; ++x)
            {
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);
            }

            for (int x = 0; x < partial_width; ++x)
                *dst_ptr++ = static_cast<TargetT>(table_[*src_ptr++]);

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    std::unique_ptr<quint32[]> table_;

    PixelFormat source_format_;
    PixelFormat target_format_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFrom8_16bppT);
};

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<PixelTranslator> PixelTranslator::create(
    const PixelFormat& source_format, const PixelFormat& target_format)
{
    switch (target_format.bytesPerPixel())
    {
        case 4:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<quint32, quint32>>(
                        source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint16, quint32>>(
                        source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint8, quint32>>(
                        source_format, target_format);

                default:
                    break;
            }
        }
        break;

        case 2:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<quint32, quint16>>(
                        source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint16, quint16>>(
                        source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint8, quint16>>(
                        source_format, target_format);

                default:
                    break;
            }
        }
        break;

        case 1:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<quint32, quint8>>(
                        source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint16, quint8>>(
                        source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<quint8, quint8>>(
                        source_format, target_format);

                default:
                    break;
            }
        }
        break;

        default:
            break;
    }

    return nullptr;
}

} // namespace base
