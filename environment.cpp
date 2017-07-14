#include "environment.h"
#include <iostream>

using namespace std;

Environment::Environment(unique_ptr<uint8_t> && _rom) : cpu({}), rom(move(_rom)) {
  cout << "Environment has been created" << endl;
}

void Environment::reset() {
  cout << "Reset" << endl;

  for (size_t i = 0; i < MEM_SIZE; i++) {
    mem[i] = 0;
  }

  cpu.reg_pc = 0;
}
