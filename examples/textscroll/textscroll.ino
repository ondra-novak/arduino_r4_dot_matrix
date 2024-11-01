#include <DotMatrix.h>

using MyFB =  DotMatrix::FrameBuffer<8, 96*6, DotMatrix::Format::monochrome_1bit>;
using MyDriver = DotMatrix::Driver<MyFB, DotMatrix::Orientation::portrait>;

MyFB myfb;
constexpr MyDriver driver = {};
DotMatrix::State st = {};


void setup() {
  char all[110] = "Hello world! ";
  for (int i = 0; i < 96; ++i) all[i+13] = i+32;
  all[110] = 0;
    DotMatrix::TextRender<DotMatrix::BltOp::copy, DotMatrix::Rotation::rot90>
      ::render_text(myfb, DotMatrix::font_6p, 7, 15, all);
}

void loop() {
  delay(2);
  driver.drive(st,myfb,st.counter / 50);

}
