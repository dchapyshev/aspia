//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/pixel_translator.h"

namespace aspia {

namespace {

template<int kSourceBpp, int kTargetBpp>
class PixelTranslatorT : public PixelTranslator
{
public:
    PixelTranslatorT(const PixelFormat& source_format, const PixelFormat& target_format)
        : source_format_(source_format),
          target_format_(target_format)
    {
        red_table_ = std::make_unique<uint32_t[]>(source_format_.redMax() + 1);
        green_table_ = std::make_unique<uint32_t[]>(source_format_.greenMax() + 1);
        blue_table_ = std::make_unique<uint32_t[]>(source_format_.blueMax() + 1);

        for (uint32_t i = 0; i <= source_format_.redMax(); ++i)
        {
            red_table_[i] = ((i * target_format_.redMax() + source_format_.redMax() / 2) /
                             source_format_.redMax()) << target_format_.redShift();
        }

        for (uint32_t i = 0; i <= source_format_.greenMax(); ++i)
        {
            green_table_[i] = ((i * target_format_.greenMax() + source_format_.greenMax() / 2) /
                               source_format_.greenMax()) << target_format_.greenShift();
        }

        for (uint32_t i = 0; i <= source_format_.blueMax(); ++i)
        {
            blue_table_[i] = ((i * target_format_.blueMax() + source_format_.blueMax() / 2) /
                              source_format_.blueMax()) << target_format_.blueShift();
        }
    }

    ~PixelTranslatorT() = default;

    void translate(const uint8_t* src, int src_stride,
                   uint8_t* dst, int dst_stride,
                   int width, int height) override
    {
        src_stride -= width * kSourceBpp;
        dst_stride -= width * kTargetBpp;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t red;
                uint32_t green;
                uint32_t blue;

                if constexpr (kSourceBpp == 4)
                {
                    red = red_table_[
                        *(uint32_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint32_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint32_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
                }
                else if constexpr (kSourceBpp == 2)
                {
                    red = red_table_[
                        *(uint16_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint16_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint16_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
                }
                else
                {
                    static_assert(kSourceBpp == 1);

                    red = red_table_[
                        *(uint8_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint8_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint8_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
                }

                if constexpr (kTargetBpp == 4)
                {
                    *(uint32_t*)dst = static_cast<uint32_t>(red | green | blue);
                }
                else if constexpr (kTargetBpp == 2)
                {
                    *(uint16_t*)dst = static_cast<uint16_t>(red | green | blue);
                }
                else
                {
                    static_assert(kTargetBpp == 1);
                    *(uint8_t*)dst = static_cast<uint8_t>(red | green | blue);
                }

                src += kSourceBpp;
                dst += kTargetBpp;
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    std::unique_ptr<uint32_t[]> red_table_;
    std::unique_ptr<uint32_t[]> green_table_;
    std::unique_ptr<uint32_t[]> blue_table_;

    PixelFormat source_format_;
    PixelFormat target_format_;

    Q_DISABLE_COPY(PixelTranslatorT)
};

template<int kSourceBpp, int kTargetBpp>
class PixelTranslatorFrom8_16bppT : public PixelTranslator
{
public:
    PixelTranslatorFrom8_16bppT(const PixelFormat& source_format, const PixelFormat& target_format)
        : source_format_(source_format),
          target_format_(target_format)
    {
        size_t table_size;

        if constexpr (kSourceBpp == 2)
        {
            table_size = 65536;
        }
        else
        {
            static_assert(kSourceBpp == 1);
            table_size = 256;
        }

        table_ = std::make_unique<uint32_t[]>(table_size);

        uint32_t source_red_mask = source_format.redMax() << source_format.redShift();
        uint32_t source_green_mask = source_format.greenMax() << source_format.greenShift();
        uint32_t source_blue_mask = source_format.blueMax() << source_format.blueShift();

        for (uint32_t i = 0; i < table_size; ++i)
        {
            uint32_t source_red = (i & source_red_mask) >> source_format.redShift();
            uint32_t source_green = (i & source_green_mask) >> source_format.greenShift();
            uint32_t source_blue = (i & source_blue_mask) >> source_format.blueShift();

            uint32_t target_red =
                (source_red * target_format.redMax() / source_format.redMax()) << target_format.redShift();
            uint32_t target_green =
                (source_green * target_format.greenMax() / source_format.greenMax()) << target_format.greenShift();
            uint32_t target_blue =
                (source_blue * target_format.blueMax() / source_format.blueMax()) << target_format.blueShift();

            table_[i] = target_red | target_green | target_blue;
        }
    }

    ~PixelTranslatorFrom8_16bppT() = default;

    void translate(const uint8_t* src, int src_stride,
                   uint8_t* dst, int dst_stride,
                   int width, int height) override
    {
        src_stride -= width * kSourceBpp;
        dst_stride -= width * kTargetBpp;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t target_pixel;

                if constexpr (kSourceBpp == 2)
                {
                    target_pixel = table_[*(uint16_t*)src];
                }
                else
                {
                    static_assert(kSourceBpp == 1);
                    target_pixel = table_[*(uint8_t*)src];
                }

                if constexpr (kTargetBpp == 4)
                {
                    *(uint32_t*)dst = static_cast<uint32_t>(target_pixel);
                }
                else if constexpr (kTargetBpp == 2)
                {
                    *(uint16_t*)dst = static_cast<uint16_t>(target_pixel);
                }
                else
                {
                    static_assert(kTargetBpp == 1);
                    *(uint8_t*)dst = static_cast<uint8_t>(target_pixel);
                }

                src += kSourceBpp;
                dst += kTargetBpp;
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    std::unique_ptr<uint32_t[]> table_;

    PixelFormat source_format_;
    PixelFormat target_format_;

    Q_DISABLE_COPY(PixelTranslatorFrom8_16bppT)
};

} // namespace

// static
std::unique_ptr<PixelTranslator> PixelTranslator::create(const PixelFormat& source_format,
                                                         const PixelFormat& target_format)
{
    switch (target_format.bytesPerPixel())
    {
        case 4:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 4>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<2, 4>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<1, 4>>(source_format, target_format);
            }
        }
        break;

        case 2:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 2>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<2, 2>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<1, 2>>(source_format, target_format);
            }
        }
        break;

        case 1:
        {
            switch (source_format.bytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 1>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<2, 1>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorFrom8_16bppT<1, 1>>(source_format, target_format);
            }
        }
        break;
    }

    return nullptr;
}

} // namespace aspia
