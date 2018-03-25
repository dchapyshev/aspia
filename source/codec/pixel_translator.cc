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
        red_table_.resize(source_format_.redMax() + 1);
        green_table_.resize(source_format_.greenMax() + 1);
        blue_table_.resize(source_format_.blueMax() + 1);

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
                        *(uint32_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint32_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint32_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
                }
                else if constexpr (source_bpp == 2)
                {
                    red = red_table_[
                        *(uint16_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint16_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint16_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
                }
                else if constexpr (source_bpp == 1)
                {
                    red = red_table_[
                        *(uint8_t*) src >> source_format_.redShift() & source_format_.redMax()];
                    green = green_table_[
                        *(uint8_t*) src >> source_format_.greenShift() & source_format_.greenMax()];
                    blue = blue_table_[
                        *(uint8_t*) src >> source_format_.blueShift() & source_format_.blueMax()];
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
                    return std::make_unique<PixelTranslatorT<2, 4>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorT<1, 4>>(source_format, target_format);
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
                    return std::make_unique<PixelTranslatorT<2, 2>>(source_format, target_format);

                case 1:
                    return std::make_unique<PixelTranslatorT<1, 2>>(source_format, target_format);
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
