#include "util.h"
#include <cstdio>
#include "defines.h"

uint8_t rotate_left_n(uint8_t val, unsigned int n) {
  const unsigned int size = sizeof(val) * 8;
  n = n % size;
  return (val >> (size - n)) | ((val & ((1 << (size - n)) - 1)) << n);
}
