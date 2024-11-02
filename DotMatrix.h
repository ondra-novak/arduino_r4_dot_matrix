#pragma once
#include <Arduino.h>
#include <type_traits>
#include <cstdint>
namespace DotMatrix {

///Define bitmap format
enum class Format {
    ///monochrome format, 1 bit, 1-on, 0-off
    monochrome_1bit,
    ///two bit format: 00-off, 01-low intensity, 10-high intensity, 11-blink high intensity
    gray_blink_2bit,
};

enum class Order {
    msb_to_lsb,     ///MSB is left, LSB is right
    lsb_to_msb      ///LSB is left, MSB is right
};

///define orientation
enum class Orientation {
    ///portrait (8x12, power/usb is at upper)
    portrait,        ///< portrait
    ///landscape (12x8, power/usb is at left)
    landscape,       ///< landscape
    ///reverse portrait (8x12, power/usb is at bottom
    reverse_portrait,///< reverse_portrait
    ///reverse landscape (12x8, power/usb is at right)
    reverse_landscape///< reverse_landscape
};

///Declaration of frame buffer
/**
 * A frame buffer is memory reserved for pixels. The frame buffer can be larger
 * than actual displayable area (12x8). This allows to create virtual screen
 *
 * @tparam _width with of the frame buffer
 * @tparam _height height of the frame buffer
 * @tparam _format specified format (see Format)
 *
 * @note with of the frame buffer is limited to 170pixels, because total of
 * displayed area cannot exeed 256 bytes (12*170/8). If you want to implement
 * scrolling text, consider to use portrait orientation (8xunlimited) and place
 * 90 degree rotated letters
 *
 * @note zero pixel (0,0) is left top. X extends right, Y extends bottom
 */
template<unsigned int _width, unsigned int _height, Format _format = Format::monochrome_1bit, Order _order = Order::msb_to_lsb>
struct FrameBuffer {
    ///width
    static constexpr unsigned int width = _width;
    ///height
    static constexpr unsigned int height = _height;
    ///format
    static constexpr Format format = _format;

    static constexpr Order order = _order;

    ///bits per pixel
    static constexpr uint8_t bits_per_pixel =
            _format == Format::monochrome_1bit?1:
            _format == Format::gray_blink_2bit?2:0;
    ///total count of pixelx of frame buffer
    static constexpr unsigned int count_pixels = width*height;
    ///total active pixels in frame (because pixels at right still need to be counted)
    static constexpr unsigned int whole_frame = 12*_width;
    ///count of bytes reserved for data
    static constexpr unsigned int count_bytes = (count_pixels*bits_per_pixel+7)/8;
    ///mask of pixel
    static constexpr uint8_t mask = (1 << bits_per_pixel) - 1;


    static_assert(bits_per_pixel > 0, "Unsupported format");
    static_assert(width > 0, "Width can't be zero");
    static_assert(height > 0, "Height can't be zero");
    static_assert(whole_frame<2048, "Too large frame");


    ///actual buffer - it is public, you can directly access
    uint8_t pixels[count_bytes];


    ///set value of pixel
    /**
     * @param x x coord
     * @param y y coord
     * @param value value, it is always masked by the mask
     */
    constexpr void set_pixel(unsigned int x, unsigned int y, uint8_t value) {
        if (x < _width && y < _height) {
            unsigned int bit = (x + y * width) * bits_per_pixel;
            unsigned int byte = bit / 8;
            unsigned int shift = bit % 8;
            auto new_val = (pixels[byte] ^ (value << shift)) & (mask << shift);
            pixels[byte] ^= new_val;
        }
    }

    ///retrieve pixel value
    /**
     * @param x x coord
     * @param y y coord
     * @return value of pixel
     */
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

///contains driver's state
/**
 * This variable contains state of driver (as the driver is read-only). You
 * need to store it along with frame-buffer if you have multiple buffers.
 *
 * You need to change blink_mask to change period of blinking
 */
struct State {
    ///count of calls
    unsigned int counter = 0;
    ///specifies mask of counter for flashing
    /** If counter & blink_mask is  non zero, blinking pixels are shown otherwise not */
    unsigned int blink_mask = 512;
};


///Declaration of the driver
/**
 * This class drives the LED matrix. You should declare it as constexpr to
 * let compiler to create necessary tables during compilation phase
 *
 * @tparam FrameBuffer type of frame buffer
 * @tparam _orientation specifies orientation.
 * @tparam _offset pixel offset in frame buffer. While working with virtual screen, you
 * can set begin of the frame buffer in step of whole byte. Which means you cannot
 * slide left and right by pixel. For monochrome, you can slide by 8 pixels. However you
 * can create 8 drivers, each driver is shifted by 1 pixel right. To sliding left and
 * right, you will need to choose appropriate driver for current sliding position.
 *
 * @note if you want to implement scrolling text, consider portrait orientation and
 * text rotated about 90 degrees. You can freely slide up and down with one driver
 */
template<typename FrameBuffer, Orientation _orientation = Orientation::portrait, int _offset = 0>
class Driver {
public:
    ///construct driver and build LED maps
    /** Please, use constexpr declaration which results by preparing maps by a compiler*/

    constexpr Driver() {build_map();}

    ///Drive the LED matrix a display something
    /**
     * To drive display, you need to call this function at least for 500x per second for
     * monochromatic format, or 1000x per seconds for gray format. Lower period
     * can cause flickering. Higher period is wasting of power and can cause dimmer
     * LED output
     *
     * @param st state of driving
     * @param fb frame buffer to display
     * @param fb_offset offset in frame buffer in bytes
     */
    void drive(State &st, const FrameBuffer &fb, unsigned int fb_offset = 0) const {
        auto c = ++st.counter;
        if constexpr(FrameBuffer::format == Format::monochrome_1bit) {
            drive_mono(c, fb, fb_offset);
        } else if constexpr(FrameBuffer::format == Format::gray_blink_2bit) {
            drive_gray(c, !(c & st.blink_mask), fb, fb_offset);
        }
    }
protected:
    struct PixelLocation {
        uint8_t offset;
        uint8_t shift = 8;
    };
    static constexpr Order order = FrameBuffer::order;
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
            if (order == Order::lsb_to_msb) {
                l.shift = (8-bits_per_pixel)-pxofs % 8;
            } else {
                l.shift = pxofs % 8;
            }
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
        bool gray_on = !(c & 1);;
        unsigned int hrow = (c >> 1) % num_rows;
        if (gray_on) {
            clear_matrix();
            activate_row(hrow, true);
            for (int i = 0; i < num_rows-1; ++i) {
                const PixelLocation &ploc = pixel_map[hrow][i];
                auto lrow = i>=hrow?i+1:i;
                unsigned int addr = (fb_offset + ploc.offset) % FrameBuffer::count_bytes;
                uint8_t b = (fb.pixels[addr] >> ploc.shift) & 3;
                switch (b) {
                    default:break;
                    case 1: [[fallthrough]];
                    case 2: activate_row(lrow, false);break;
                    case 3: if (flash) activate_row(lrow, false);break;;
                }
            }
        } else {
            for (int i = 0; i < num_rows-1; ++i) {
                const PixelLocation &ploc = pixel_map[hrow][i];
                auto lrow = i>=hrow?i+1:i;
                unsigned int addr = (fb_offset + ploc.offset) % FrameBuffer::count_bytes;
                uint8_t b = (fb.pixels[addr] >> ploc.shift) & 3;
                if (b == 1) {
                    deactivate_row(lrow);
                }
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
    void deactivate_row(int row) const {
      bsp_io_port_pin_t pin_a = g_pin_cfg[row + pin_zero_index].pin;
      if (pin_a >> 8) {
          R_PORT2->PCNTR1 &= ~(1 << (pin_a & 0xFF));
      } else {
          R_PORT0->PCNTR1 &= ~(1 << (pin_a & 0xFF));
      }
    }
};

}
#include "bitmap.h"
#include "font_6p.h"
#include "font_5x3.h"
