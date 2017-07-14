#pragma once

#include "cpu.h"
#include <cstdint>
#include "defines.h"
#include <memory>

using namespace std;

class Environment {
public:
  Environment(unique_ptr<uint8_t>&&);
  void reset();

private:
  CPU cpu;
  unique_ptr<uint8_t> rom;

  // 0x0000-0x3FFF: Permanently-mapped ROM bank.
  // 0x4000-0x7FFF: Area for switchable ROM banks.
  // 0x8000-0x9FFF: Video RAM.
  // 0xA000-0xBFFF: Area for switchable external RAM banks.
  // 0xC000-0xCFFF: Game Boy’s working RAM bank 0 .
  // 0xD000-0xDFFF: Game Boy’s working RAM bank 1.
  // 0xFE00-0xFEFF: Sprite Attribute Table.
  // 0xFF00-0xFF7F: Devices’ Mappings. Used to access I/O devices.
  // 0xFF80-0xFFFE: High RAM Area.
  // 0xFFFF: Interrupt Enable Register.
  uint8_t mem[MEM_SIZE];
};
