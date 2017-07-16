#include "tests.h"
#include "util.h"
#include "defines.h"
#include <cassert>

void run_test() {
  assert(rotate_left(0b11011100)  == 0b10111001);
  assert(rotate_right(0b11011100) == 0b01101110);

  assert(BITN(0b1011, 0) == 1);
  assert(BITN(0b1011, 1) == 1);
  assert(BITN(0b1011, 2) == 0);
  assert(BITN(0b1011, 3) == 1);


  assert(ISBITN(0b1011,  0));
  assert(ISBITN(0b1011,  1));
  assert(!ISBITN(0b1011, 2));
  assert(ISBITN(0b1011,  3));

  uint8_t vui8 = 0b10001111;
  assert((uint8_t) vui8 > 0);
  assert((char) vui8 < 0);
}
