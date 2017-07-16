#pragma once

#include <cstdint>

using namespace std;

class CPU {
public:
  CPU();

  // Flag F:
  // Bits 0 to 3 are not used.
  // Bit 4 represents the carry flag.  It is set when a carry from bit 7 is produced in arithmetical instructions.  Otherwise it is cleared.
  // Bit 5 represents the half-carry flag.  It is set when a carry from bit 3 is produced in arithmetical instructions.  Otherwise it is cleared.  It has a very common use, that is, for the DAA (decimal adjust) instruction.  Games used it extensively for displaying decimal values on the screen.
  // Bit 6 represents the subtract flag.  When the instruction is a subtraction this bit is set.  Otherwise (the instruction is an addition) it is cleared.
  // Bit 7 represents the zero flag.  It is set when the instruction results in a value of 0.  Otherwise (result different to 0) it is cleared.
  uint8_t reg_a, reg_f, // F bits: Z(7) zero, N(6) substract, H(5) half carry, C(4) carry.
          reg_b, reg_c,
          reg_d, reg_e,
          reg_h, reg_l;

  uint16_t reg_sp, reg_pc;

  void dump_registers();
  void step_dword_reg(uint8_t *, uint8_t *, int);
  void dec_hl();
  void inc_hl();

  uint16_t af();
  uint16_t bc();
  uint16_t de();
  uint16_t hl();

  void set_reg_pair(uint8_t *, uint8_t *, uint16_t);
  void set_af(uint16_t);
  void set_bc(uint16_t);
  void set_de(uint16_t);
  void set_hl(uint16_t);
};

template <typename T>
void dump_bin(T);
