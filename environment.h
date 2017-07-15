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
  void run();

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

  uint8_t get_mem(uint16_t);
  void set_mem(uint16_t, uint8_t);
  uint8_t read_next();
  uint16_t read_next_hl();

  inline void set_flag(uint8_t, bool);
  void set_zero_flag(bool);
  void set_substract_flag(bool);
  void set_half_carry_flag(bool);
  void set_carry_flag(bool);
};
