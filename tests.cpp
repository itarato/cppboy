#include "tests.h"
#include "util.h"
#include "defines.h"
#include <cassert>

void run_test() {
  assert(rotate_left_n(0b11011100, 0) == 0b11011100);
  assert(rotate_left_n(0b11011100, 1) == 0b10111001);
  assert(rotate_left_n(0b11011100, 2) == 0b01110011);
  assert(rotate_left_n(0b11011100, 3) == 0b11100110);
  assert(rotate_left_n(0b11011100, 4) == 0b11001101);
  assert(rotate_left_n(0b11011100, 5) == 0b10011011);
  assert(rotate_left_n(0b11011100, 6) == 0b00110111);
  assert(rotate_left_n(0b11011100, 7) == 0b01101110);
  assert(rotate_left_n(0b11011100, 8) == 0b11011100);

  assert(BITN(0b1011, 0) == 1);
  assert(BITN(0b1011, 1) == 1);
  assert(BITN(0b1011, 2) == 0);
  assert(BITN(0b1011, 3) == 1);


  assert(ISBITN(0b1011, 0));
  assert(ISBITN(0b1011, 1));
  assert(!ISBITN(0b1011, 2));
  assert(ISBITN(0b1011, 3));

  uint8_t vui8 = 0b10001111;
  assert((uint8_t) vui8 > 0);
  assert((char) vui8 < 0);
}
