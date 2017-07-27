#include "util.h"
#include <cstdio>
#include <iostream>
#include "defines.h"

using namespace std;

uint8_t rotate_left(uint8_t val) {
  return val << 1 | BITN(val, 7);
}

uint8_t rotate_right(uint8_t val) {
  return val >> 1 | (BITN(val, 0) << 7);
}
