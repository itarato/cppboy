#include <iostream>
#include <fstream>
#include <string>
#include "environment.h"
#include "tests.h"
#include "defines.h"

using namespace std;

int main() {
  cout << "Executing tests." << endl;
  run_test();

  cout << "Reading ROM" << endl;
  string rom_file_name = "rom.bin";
  fstream rom_file(rom_file_name, ios::binary | ios::in);
  unique_ptr<uint8_t> rom(new uint8_t[ROM_SIZE]);
  rom_file.read(reinterpret_cast<char *>(rom.get()), ROM_SIZE);

  for (int i = 0; i < ROM_SIZE; i++) {
    printf("%.2x ", *(rom.get() + i));
    if (i % 0x10 == 0xF) cout << endl;
  }

  Environment env{move(rom)};
  env.reset();
  env.run();

  cout << "End" << endl;
  return EXIT_SUCCESS;
}
