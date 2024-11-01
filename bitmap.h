#pragma once
#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>
namespace DotMatrix {
enum class BltOp {
    copy, and_op, or_op, xor_op
};
struct ColorMap {
    uint8_t foreground = 0xFF;
    uint8_t background = 0x00;
};
template<int width, int height>
class Bitmap {
public:
    static constexpr auto w = width;
    static constexpr auto h = height;
    static constexpr int get_width() {
        return width;
    }
    static constexpr int get_height() {
        return height;
    }
    using RowType = std::conditional_t<width>=16,uint32_t,
    std::conditional_t<width>=8,uint16_t,uint8_t> >;
    constexpr void set_pixel(int x, int y) {
        bitmap[y] |= static_cast<RowType>(1) << x;
    }
    constexpr bool get_pixel(int x, int y) const {
        return (bitmap[y] & (1 << x)) != 0;
    }
    constexpr Bitmap(const char *asciiart) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (*asciiart > 32)
                    set_pixel(x, y);
                ++asciiart;
            }
        }
        if (*asciiart != 0) {
            bitmap[height] = 1;
        }
    }
protected:
    RowType bitmap[height] = { };
};
template<BltOp op = BltOp::copy, Orientation o = Orientation::landscape>
struct BitBlt {
    template <typename Bitmap, typename FrameBuffer>
    static constexpr void bitblt(const Bitmap &bm, FrameBuffer &fb, int col, int row,
        const ColorMap &colors = { }) {
        auto h = bm.get_height();
        auto w = bm.get_width();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                unsigned int r = 0;
                unsigned int c = 0;
                if constexpr(o == Orientation::landscape) {
                    r = y + row;
                    c = x + col;
                } else if constexpr(o == Orientation::portrait) {
                    r = x + row;
                    c = col - y;
                } else if constexpr(o == Orientation::reverse_landscape) {
                    r = row - y;
                    c = col - x;
                } else if constexpr(o == Orientation::reverse_portrait) {
                    r = row - x;
                    c = y + col;
                }
                auto v = bm.get_pixel(x, y);
                if constexpr (op == BltOp::copy) {
                    fb.set_pixel(c, r, v ? colors.foreground : colors.background);
                } else if constexpr (op == BltOp::and_op) {
                    if (!v)
                        fb.set_pixel(c, r, colors.background);
                } else if constexpr (op == BltOp::or_op) {
                    if (v)
                        fb.set_pixel(c, r, colors.foreground);
                } else {
                    auto cc = fb.get_pixel(c, r);
                    fb.set_pixel(c, r,
                            cc ^ (v ? colors.foreground : colors.background));
                }
            }
        }
    }
};
class BitmapView {
public:
    int get_width() const {
        return width;
    }
    int get_height() const {
        return height;
    }
    uint8_t get_pixel(int w, int h) const {
        return get_pixel_fn(*this, w, h);
    }
    template<int _w, int _h>
    BitmapView(const Bitmap<_w, _h> &bp) :
            width(_w), height(_h), ref(&bp), get_pixel_fn(
                    [](const BitmapView &me, int w, int h) {
                        auto b = reinterpret_cast<const Bitmap<_h, _w>*>(me.ref);
                        if (w < _w && h < _h) {
                            return b->set_pixel(w, h);
                        }
                        return false;
                    }) {
    }
protected:
    int width;
    int height;
    const void *ref;
    uint8_t (*get_pixel_fn)(const BitmapView&, int, int) = nullptr;
};
template<int height, int width>
using FontFace = Bitmap<width,height>;
template<int height, int max_width = 8>
struct FontFaceP {
    uint8_t width;
    Bitmap<max_width,height> face;
};
template<int height, int width>
using Font = std::array<FontFace<height, width>, 96>;
template<int height, int max_width>
using FontP = std::array<FontFaceP<height, max_width>, 96>;
template<typename T>
struct FontFaceSpec;
template<int height, int max_width>
struct FontFaceSpec<FontFaceP<height, max_width> > {
    const FontFaceP<height, max_width> &ch;
     constexpr uint8_t get_width() const {
        return ch.width;
    }
     constexpr const Bitmap<max_width, height> &get_face() const {
        return ch.face;
    }
};
template<int height, int width>
struct FontFaceSpec<FontFace<height, width> > {
    const FontFace<height, width> &ch;
     constexpr uint8_t get_width() const{
        return width;
    }
     constexpr const Bitmap<width, height> &get_face() const {
        return ch;
    }
};
template<typename Font, typename Fn>
auto do_for_character(const Font &font, int ascii_char, Fn &&fn) {
    if (ascii_char < 33) ascii_char = 32;
    else if (ascii_char > 127) ascii_char = '?';
    ascii_char -= 32;
    const auto &chdef = font[ascii_char];
    using Spec = FontFaceSpec<std::decay_t<decltype(chdef)> >;
    return fn(Spec{chdef});
}
template<BltOp op = BltOp::copy, Orientation o = Orientation::landscape>
struct TextRender {
    template<typename FrameBuffer, typename Font>
    static uint8_t render_character(FrameBuffer &fb, const Font &font,
            unsigned int x, unsigned int y, int ascii_char, const ColorMap &cols) {
        return do_for_character(font, ascii_char, [&](auto spec){
            BitBlt<op, o>::template bitblt(spec.get_face() ,fb, x, y, cols);
            return spec.get_width();
        });
    }
    template<typename Font>
    static uint8_t get_character_width(const Font &font, int ascii_char) {
        return do_for_character(font, ascii_char, [&](auto spec){
            spec.get_width();
        });
    }
    template<typename FrameBuffer, typename Font>
    static std::pair<unsigned int, unsigned int> render_text(FrameBuffer &fb, const Font &font,
            unsigned int x, unsigned int y, std::string_view text, const ColorMap &cols = {}) {
        for (char c: text) {
            auto s = render_character(fb, font, x, y, c, cols);
            if constexpr(o == Orientation::landscape) {
                x+=s;
            } else if constexpr(o == Orientation::portrait) {
                y+=s;
            } else if constexpr(o == Orientation::reverse_landscape) {
                x-=s;
            } else if constexpr(o == Orientation::reverse_portrait) {
                y-=s;
            }
        }
        return {x,y};
    }
};
}
