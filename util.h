#pragma once

#include <cstdint>
#include <iostream>

using namespace std;

uint8_t rotate_left(uint8_t);
uint8_t rotate_right(uint8_t);

template <typename T> void dump_bin(T);

template <typename T>
void dump_bin(T val) {
  size_t len = sizeof(T);

  for (int i = len * 8 - 1; i >= 0; i--) {
    cout << ((1 << i) & val ? 1 : 0);
    if (i == 4) cout << '_';
  }
}
