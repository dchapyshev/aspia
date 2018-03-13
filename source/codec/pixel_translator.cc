//
// PROJECT:         Aspia
// FILE:            codec/pixel_translator.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/pixel_translator.h"
#include "base/macros.h"

namespace aspia {

namespace {

template<int source_bpp, int target_bpp>
class PixelTranslatorT : public PixelTranslator
{
public:
    PixelTranslatorT(const PixelFormat& source_format, const PixelFormat& target_format)
        : source_format_(source_format),
          target_format_(target_format)
    {
        red_table_.resize(source_format_.RedMax() + 1);
        green_table_.resize(source_format_.GreenMax() + 1);
        blue_table_.resize(source_format_.BlueMax() + 1);

        for (uint32_t i = 0; i <= source_format_.RedMax(); ++i)
        {
            red_table_[i] = ((i * target_format_.RedMax() + source_format_.RedMax() / 2) /
                             source_format_.RedMax()) << target_format_.RedShift();
        }

        for (uint32_t i = 0; i <= source_format_.GreenMax(); ++i)
        {
            green_table_[i] = ((i * target_format_.GreenMax() + source_format_.GreenMax() / 2) /
                               source_format_.GreenMax()) << target_format_.GreenShift();
        }

        for (uint32_t i = 0; i <= source_format_.BlueMax(); ++i)
        {
            blue_table_[i] = ((i * target_format_.BlueMax() + source_format_.BlueMax() / 2) /
                              source_format_.BlueMax()) << target_format_.BlueShift();
        }
    }

    ~PixelTranslatorT() = default;

    void Translate(const uint8_t* src, int src_stride,
                   uint8_t* dst, int dst_stride,
                   int width, int height) override
    {
        src_stride -= width * source_bpp;
        dst_stride -= width * target_bpp;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t red;
                uint32_t green;
                uint32_t blue;

                if constexpr (source_bpp == 4)
                {
                    red = red_table_[
                        *(uint32_t*) src >> source_format_.RedShift() & source_format_.RedMax()];
                    green = green_table_[
                        *(uint32_t*) src >> source_format_.GreenShift() & source_format_.GreenMax()];
                    blue = blue_table_[
                        *(uint32_t*) src >> source_format_.BlueShift() & source_format_.BlueMax()];
                }
                else if constexpr (source_bpp == 2)
                {
                    red = red_table_[
                        *(uint16_t*) src >> source_format_.RedShift() & source_format_.RedMax()];
                    green = green_table_[
                        *(uint16_t*) src >> source_format_.GreenShift() & source_format_.GreenMax()];
                    blue = blue_table_[
                        *(uint16_t*) src >> source_format_.BlueShift() & source_format_.BlueMax()];
                }
                else if constexpr (source_bpp == 1)
                {
                    red = red_table_[
                        *(uint8_t*) src >> source_format_.RedShift() & source_format_.RedMax()];
                    green = green_table_[
                        *(uint8_t*) src >> source_format_.GreenShift() & source_format_.GreenMax()];
                    blue = blue_table_[
                        *(uint8_t*) src >> source_format_.BlueShift() & source_format_.BlueMax()];
                }

                if constexpr (target_bpp == 4)
                    *(uint32_t*) dst = static_cast<uint32_t>(red | green | blue);
                else if constexpr (target_bpp == 2)
                    *(uint16_t*) dst = static_cast<uint16_t>(red | green | blue);
                else if constexpr (target_bpp == 1)
                    *(uint8_t*) dst = static_cast<uint8_t>(red | green | blue);

                src += source_bpp;
                dst += target_bpp;
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    std::vector<uint32_t> red_table_;
    std::vector<uint32_t> green_table_;
    std::vector<uint32_t> blue_table_;

    PixelFormat source_format_;
    PixelFormat target_format_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorT);
};

} // namespace

// static
std::unique_ptr<PixelTranslator> PixelTranslator::Create(const PixelFormat& source_format,
                                                         const PixelFormat& target_format)
{
    switch (target_format.BytesPerPixel())
    {
        case 4:
        {
            switch (source_format.BytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 4>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorT<2, 4>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorT<1, 4>>(source_format, target_format);
            }
        }
        break;

        case 2:
        {
            switch (source_format.BytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 2>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorT<2, 2>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorT<1, 2>>(source_format, target_format);
            }
        }
        break;

        case 1:
        {
            switch (source_format.BytesPerPixel())
            {
                case 4:
                    return std::make_unique<PixelTranslatorT<4, 1>>(source_format, target_format);

                case 2:
                    return std::make_unique<PixelTranslatorT<2, 1>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorT<1, 1>>(source_format, target_format);
            }
        }
        break;
    }

    return nullptr;
}

} // namespace aspia
