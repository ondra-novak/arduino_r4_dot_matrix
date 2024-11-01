#include <DotMatrix.h>

using MyFB =  DotMatrix::FrameBuffer<12, 8, DotMatrix::Format::gray_blink_2bit>;
using MyDriver = DotMatrix::Driver<MyFB, DotMatrix::Orientation::landscape>;
using TextRender =  DotMatrix::TextRender<DotMatrix::BltOp::copy, DotMatrix::Rotation::rot0>;

MyFB framebuffer;
DotMatrix::State st;
constexpr MyDriver dotmx_driver = {};
constexpr auto font_5x3 = DotMatrix::font_5x3;

void setup() {
  TextRender::render_character(framebuffer, font_5x3, 0, 0, '1',{1,0});
  TextRender::render_character(framebuffer, font_5x3, 5, 0, '2',{2,0});
  TextRender::render_character(framebuffer, font_5x3, 9, 0, '3',{3,0});
  for (int x = 0; x < 12; ++x) {
    for (int y = 6; y < 8; ++y) {
      framebuffer.set_pixel(x, y, x/4+1);
    }
  }

}

void loop() {
  delay(1);
  dotmx_driver.drive(st, framebuffer);
}
