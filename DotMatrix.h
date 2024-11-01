#pragma once
#include <Arduino.h>
#include <type_traits>
#include <cstdint>
namespace DotMatrix {
enum class Format {
    monochrome,
    gray,
};
enum class Orientation {
    portrait,
    landscape,
    reverse_portrait,
    reverse_landscape
};
template<unsigned int _width, unsigned int _height, Format _format>
struct FrameBuffer {
    static constexpr unsigned int width = _width;
    static constexpr unsigned int height = _height;
    static constexpr Format format = _format;
    static constexpr uint8_t bits_per_pixel =
            _format == Format::monochrome?1:
            _format == Format::gray?2:0;
    static constexpr unsigned int count_pixels = width*height;
    static constexpr unsigned int whole_frame = 12*_width;
    static constexpr unsigned int count_bytes = (count_pixels*bits_per_pixel+7)/8;
    static constexpr uint8_t mask = (1 << bits_per_pixel) - 1;
    static_assert(bits_per_pixel > 0, "Unsupported format");
    static_assert(width > 0, "Width can't be zero");
    static_assert(height > 0, "Height can't be zero");
    static_assert(whole_frame<2048, "Too large frame");
    uint8_t pixels[count_bytes];
    constexpr void set_pixel(unsigned int x, unsigned int y, uint8_t value) {
        if (x < _width && y < _height) {
            unsigned int bit = (x + y * width) * bits_per_pixel;
            unsigned int byte = bit / 8;
            unsigned int shift = bit % 8;
            auto new_val = (pixels[byte] ^ (value << shift)) & (mask << shift);
            pixels[byte] ^= new_val;
        }
    }
    constexpr uint8_t get_pixel(unsigned int x, unsigned int y) {
        if (x < _width && y < _height) {
            unsigned int bit = (x + y * width) * bits_per_pixel;
            unsigned int byte = bit / 8;
            unsigned int shift = bit % 8;
            return (pixels[byte] >> shift) & mask;
        } else {
            return {};
        }
    }
};
template<typename X>
class IsFrameBuffer {
    static constexpr bool value = false;
};
struct State {
    unsigned int counter = 0;
    unsigned int flash_mask = 512;
};
template<typename FrameBuffer, Orientation _orientation = Orientation::portrait, int _offset = 0>
class Driver {
public:
    constexpr Driver() {build_map();}
    void drive(State &st, const FrameBuffer &fb, unsigned int fb_offset = 0) const {
        auto c = ++st.counter;
        if constexpr(FrameBuffer::format == Format::monochrome) {
            drive_mono(c, fb, fb_offset);
        } else if constexpr(FrameBuffer::format == Format::gray) {
            drive_gray(c, !(c & st.flash_mask), fb, fb_offset);
        }
    }
protected:
    struct PixelLocation {
        uint8_t offset;
        uint8_t shift;
    };
    static constexpr unsigned int bits_per_pixel = FrameBuffer::bits_per_pixel;
    static constexpr unsigned int fb_width = FrameBuffer::width;
    static constexpr uint8_t mask = FrameBuffer::mask;
    static constexpr unsigned int num_leds = 96;
    static constexpr unsigned int num_rows = 11;
    static constexpr unsigned int pin_zero_index = 28;
    static constexpr uint8_t pins[num_leds][2] = {
            { 7, 3 }, { 3, 7 }, { 7, 4 },
            { 4, 7 }, { 3, 4 }, { 4, 3 }, { 7, 8 }, { 8, 7 }, { 3, 8 },
            { 8, 3 }, { 4, 8 }, { 8, 4 }, { 7, 0 }, { 0, 7 }, { 3, 0 },
            { 0, 3 }, { 4, 0 }, { 0, 4 }, { 8, 0 }, { 0, 8 }, { 7, 6 },
            { 6, 7 }, { 3, 6 }, { 6, 3 }, { 4, 6 }, { 6, 4 }, { 8, 6 },
            { 6, 8 }, { 0, 6 }, { 6, 0 }, { 7, 5 }, { 5, 7 }, { 3, 5 },
            { 5, 3 }, { 4, 5 }, { 5, 4 }, { 8, 5 }, { 5, 8 }, { 0, 5 },
            { 5, 0 }, { 6, 5 }, { 5, 6 }, { 7, 1 }, { 1, 7 }, { 3, 1 },
            { 1, 3 }, { 4, 1 }, { 1, 4 }, { 8, 1 }, { 1, 8 }, { 0, 1 },
            { 1, 0 }, { 6, 1 }, { 1, 6 }, { 5, 1 }, { 1, 5 }, { 7, 2 },
            { 2, 7 }, { 3, 2 }, { 2, 3 }, { 4, 2 }, { 2, 4 }, { 8, 2 },
            { 2, 8 }, { 0, 2 }, { 2, 0 }, { 6, 2 }, { 2, 6 }, { 5, 2 },
            { 2, 5 }, { 1, 2 }, { 2, 1 }, { 7, 10 }, { 10, 7 }, { 3, 10 },
            { 10,3 }, { 4, 10 }, { 10, 4 }, { 8, 10 }, { 10, 8 }, { 0, 10 },
            { 10, 0 }, { 6, 10 }, { 10, 6 }, { 5, 10 }, { 10, 5 }, { 1, 10 },
            { 10, 1 }, { 2, 10 }, { 10, 2 }, { 7, 9 }, { 9, 7 }, { 3, 9 },
            { 9, 3 }, { 4, 9 }, { 9, 4 }, };
    PixelLocation pixel_map[num_rows][num_rows-1] = {};
    constexpr void build_map() {
        unsigned int px = 0;
        for (const auto &p: pins) {
            auto row = p[0];
            auto col = p[1];
            if (col > row) --col;
            PixelLocation &l = pixel_map[row][col];
            unsigned int x = px % 12;
            unsigned int y = px / 12;
            unsigned int pxofs = 0;
            if constexpr(_orientation == Orientation::landscape) {
                pxofs = y * fb_width + x + _offset;
            } else if constexpr(_orientation == Orientation::portrait) {
                pxofs = (7-y)  + x * fb_width + _offset;
            } else if constexpr(_orientation == Orientation::reverse_landscape) {
                pxofs = (7-y) * fb_width + (11-x) + _offset;
            } else if constexpr(_orientation == Orientation::reverse_portrait) {
                pxofs = y * fb_width + (11-x) * fb_width + _offset;
            }
            pxofs *= bits_per_pixel;
            l.offset = pxofs / 8;
            l.shift = pxofs % 8;
            ++px;
        }
    }
    void drive_mono(unsigned int c, const FrameBuffer &fb, unsigned int fb_offset) const {
        clear_matrix();
        unsigned int hrow = c % num_rows;
        activate_row(hrow, true);
        for (int i = 0; i < num_rows-1; ++i) {
            const PixelLocation &ploc = pixel_map[hrow][i];
            auto lrow = i>=hrow?i+1:i;
            unsigned int addr = (fb_offset + ploc.offset) % FrameBuffer::count_bytes;
            uint8_t b = (fb.pixels[addr] >> ploc.shift) & 1;
            if (b) activate_row(lrow, false);
        }
    }
    void drive_gray(unsigned int c, bool flash, const FrameBuffer &fb, unsigned int fb_offset) const {
        clear_matrix();
        bool gray_on = !(c & 1);
        unsigned int hrow = (c >> 1) % num_rows;
        activate_row(hrow, true);
        for (int i = 0; i < num_rows-1; ++i) {
            const PixelLocation &ploc = pixel_map[hrow][i];
            auto lrow = i>=hrow?i+1:i;
            unsigned int addr = (fb_offset + ploc.offset) % FrameBuffer::count_bytes;
            uint8_t b = (fb.pixels[addr] >> ploc.shift) & 3;
            switch (b) {
                default:break;
                case 1: if (gray_on) activate_row(lrow, false);break;
                case 2: activate_row(lrow, false);break;
                case 3: if (flash) activate_row(lrow, false);break;;
            }
        }
    }
    static constexpr uint32_t LED_MATRIX_PORT0_MASK  = ((1 << 3) | (1 << 4) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 15));
    static constexpr uint32_t LED_MATRIX_PORT2_MASK  = ((1 << 4) | (1 << 5) | (1 << 6) | (1 << 12) | (1 << 13));
    void clear_matrix() const {
      R_PORT0->PCNTR1 &= ~LED_MATRIX_PORT0_MASK;
      R_PORT2->PCNTR1 &= ~LED_MATRIX_PORT2_MASK;
    }
    void activate_row(int row, bool high) const {
      bsp_io_port_pin_t pin_a = g_pin_cfg[row + pin_zero_index].pin;
      R_PFS->PORT[pin_a >> 8].PIN[pin_a & 0xFF].PmnPFS =
        IOPORT_CFG_PORT_DIRECTION_OUTPUT | (high?IOPORT_CFG_PORT_OUTPUT_HIGH:IOPORT_CFG_PORT_OUTPUT_LOW);
    }
};

}
#include "bitmap.h"
#include "font_6p.h"
#include "font_5x3.h"
