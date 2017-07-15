#include "cpu.h"
#include <iostream>

using namespace std;

CPU::CPU() {
  cout << "CPU has been created" << endl;
}

void CPU::dump_registers() {
  printf("  ┏━━━━━┳━━━━━┓\n");
  printf(" A┃%5d┃%5d┃F=[", reg_a, reg_f);
  dump_bin(reg_f);
  printf("]\n");
  printf(" B┃%5d┃%5d┃C\n", reg_b, reg_c);
  printf(" D┃%5d┃%5d┃E\n", reg_d, reg_e);
  printf(" H┃%5d┃%5d┃L\n", reg_h, reg_l);
  printf("  ┣━━━━━╋━━━━━┫\n");
  printf("SP┃%5d┃%5d┃PC\n", reg_sp, reg_pc);
  printf("  ┗━━━━━┻━━━━━┛\n");
}

template <typename T>
void dump_bin(T val) {
  size_t len = sizeof(T);

  for (int i = len * 8 - 1; i >= 0; i--) {
    cout << ((1 << i) & val ? 1 : 0);
    if (i == 4) cout << '_';
  }
}
