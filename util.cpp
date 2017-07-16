#include "util.h"
#include <cstdio>
#include "defines.h"

uint8_t rotate_left(uint8_t val) {
  return val << 1 | BITN(val, 7);
}

uint8_t rotate_right(uint8_t val) {
  return val >> 1 | (BITN(val, 0) << 7);
}
