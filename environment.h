#pragma once

#include "cpu.h"
#include <cstdint>
#include "defines.h"
#include <memory>
#include "debugger.h"

using namespace std;

class Environment {
public:
  Environment(unique_ptr<uint8_t>&&);
  void reset();
  void run();

private:
  CPU cpu;
  unique_ptr<uint8_t> rom;
  uint64_t t;
  uint8_t t_div;
  uint16_t t_tima;
  Debugger dbg;

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

  uint8_t   get_mem(uint16_t);
  uint8_t * get_mem_ptr(uint16_t);
  void      set_mem(uint16_t, uint8_t);
  uint8_t   read_next();
  uint16_t  read_next_hl();

  inline void set_flag(uint8_t, bool);
  void set_zero_flag(bool);
  void set_substract_flag(bool);
  void set_half_carry_flag(bool);
  void set_carry_flag(bool);

  void push_to_stack_d8(uint8_t);
  void push_to_stack_d16(uint16_t);
  uint8_t  pop_from_stack_d8();
  uint16_t pop_from_stack_d16();

  void handle_timer_counter(uint8_t);
  void handle_interrupt();

  void op_bit_n_d8(uint8_t, uint8_t *, unsigned int);
  // @todo op inc and dec should include &dur
  void op_inc(uint8_t *);
  void op_dec(uint8_t *);
  void op_rlc_n(uint8_t *, uint8_t *);
  void op_rrc_n(uint8_t *, uint8_t *);
  void op_rl_n(uint8_t *, uint8_t *);
  void op_rr_n(uint8_t *, uint8_t *);
};
