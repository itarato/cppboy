#include "environment.h"
#include <iostream>
#include "util.h"

using namespace std;

Environment::Environment(unique_ptr<uint8_t> && _rom) : cpu({}), rom(move(_rom)), dbg({}) {
  cout << "Environment has been created" << endl;
}

void Environment::reset() {
  cout << "Reset" << endl;

  for (size_t i = 0; i < MEM_SIZE; i++) {
    mem[i] = 0;
  }

  cpu.reg_pc = 0;
  t = 0;
  t_div = 0;
  t_tima = 0;
}

uint8_t Environment::read_next() {
  uint8_t word = get_mem(cpu.reg_pc);
  cpu.reg_pc++;
  return word;
}

uint16_t Environment::read_next_hl() {
  uint16_t dword = read_next();
  dword |= read_next() << 8;
  return dword;
}

uint8_t Environment::get_mem(uint16_t ptr) {
  if (0 <= ptr && ptr < ROM_SIZE) {
    return rom.get()[ptr];
  } else {
    return mem[ptr];
  }
}

uint8_t * Environment::get_mem_ptr(uint16_t ptr) {
  if (0 <= ptr && ptr < ROM_SIZE) {
    return &rom.get()[ptr];
  } else {
    return &mem[ptr];
  }
}

void Environment::set_mem(uint16_t addr, uint8_t val) {
  if (0xC000 <= addr && addr < 0xDE00) {
    uint16_t offset = addr - 0xC000;
    mem[addr] = mem[0xE000 + offset] = val;
  } else if (0xE000 <= addr && addr < 0xFE00) {
    uint16_t offset = addr - 0xE000;
    mem[addr] = mem[0xC000 + offset] = val;
  } else if (addr == ADDR_DIV) {
    mem[addr] = 0;
  } else {
    mem[addr] = val;
  }
}

inline void Environment::set_flag(uint8_t bit, bool is_on) {
  if (is_on) {
    cpu.reg_f |= 1 << bit;
  } else {
    cpu.reg_f &= 0b11111111 ^ (1 << bit);
  }
}

void Environment::set_zero_flag(bool is_on) {
  set_flag(7, is_on);
}

void Environment::set_substract_flag(bool is_on) {
  set_flag(6, is_on);
}

void Environment::set_half_carry_flag(bool is_on) {
  set_flag(5, is_on);
}

void Environment::set_carry_flag(bool is_on) {
  set_flag(4, is_on);
}

void Environment::push_to_stack_d8(uint8_t val) {
  set_mem(cpu.reg_sp, val);
  cpu.reg_sp--;
}

void Environment::push_to_stack_d16(uint16_t val) {
  uint8_t lo = val & 0xFF;
  uint8_t hi = val >> 8;
  push_to_stack_d8(hi);
  push_to_stack_d8(lo);
}

uint8_t Environment::pop_from_stack_d8() {
  return get_mem(cpu.reg_sp++);
}

uint16_t Environment::pop_from_stack_d16() {
  uint8_t lo = pop_from_stack_d8();
  uint8_t hi = pop_from_stack_d8();
  return lo | (hi << 8);
}

void Environment::handle_timer_counter(uint8_t dur) {
  uint8_t tac = get_mem(ADDR_TAC);
  bool t_start = ISBITN(tac, 2);

  if (!t_start) return;

  uint8_t hz_mode = tac & 0b11;
  uint16_t cycles;
  switch (hz_mode) {
    case 0b00:
      cycles = 0x400;
      break;
    case 0b01:
      cycles = 0x10;
      break;
    case 0b10:
      cycles = 0x40;
      break;
    case 0b11:
      cycles = 0x100;
      break;
    default:
      ERR(printf("Unknown TAC clock input mode from TAC: 0x%x", tac));
      exit(EXIT_FAILURE);
  }

  if (t_tima + dur >= cycles) {
    if (get_mem(ADDR_TIMA) == 0xFF) {
      // INT 50 Timer Interrupt.
      set_mem(ADDR_IF, get_mem(ADDR_IF) | 0b001);
    }

    mem[ADDR_TIMA]++;
  }

  t_tima = (t_tima + dur) % cycles;
}

void Environment::handle_interrupt() {

}

void Environment::op_bit_n_d8(uint8_t word, uint8_t *dur, unsigned int n) {
  set_zero_flag(!ISBITN(word, n));
  set_substract_flag(false);
  set_half_carry_flag(true);
  *dur = 8;
}

void Environment::op_inc(uint8_t *reg) {
  *reg += 1;
  set_zero_flag(*reg == 0);
  set_substract_flag(false);
  set_half_carry_flag((*reg & 0b1111) == 0);
}

void Environment::op_dec(uint8_t *reg) {
  *reg -= 1;
  set_zero_flag(*reg == 0);
  set_substract_flag(true);
  set_half_carry_flag((*reg ^ 0b1111) == 0);
}

void Environment::op_rlc_n(uint8_t *reg, uint8_t *dur) {
  set_carry_flag(ISBITN(*reg, 7));
  set_substract_flag(false);
  set_half_carry_flag(false);

  *reg = rotate_left(*reg);
  set_zero_flag(*reg == 0);

  *dur = 8;
}

void Environment::op_rrc_n(uint8_t *reg, uint8_t *dur) {
  set_carry_flag(ISBITN(*reg, 0));
  set_substract_flag(false);
  set_half_carry_flag(false);

  *reg = rotate_right(*reg);
  set_zero_flag(*reg == 0);

  *dur = 8;
}

void Environment::op_rl_n(uint8_t *reg, uint8_t *dur) {
  *dur = 8;

  uint8_t old_carry = BITN(cpu.reg_f, BITFC);
  set_carry_flag(ISBITN(*reg, 7));
  set_substract_flag(false);
  set_half_carry_flag(false);

  *reg = (*reg << 1) | old_carry;
  set_zero_flag(*reg == 0);
}

void Environment::op_rr_n(uint8_t *reg, uint8_t *dur) {
  *dur = 8;

  uint8_t old_carry = BITN(cpu.reg_f, BITFC);
  set_carry_flag(ISBITN(*reg, 0));
  set_substract_flag(false);
  set_half_carry_flag(false);

  *reg = (*reg >> 1) | (old_carry << 7);
  set_zero_flag(*reg == 0);
}

void Environment::run() {
  uint64_t cycle = 0;
  for (;;) {
    uint8_t cmd = read_next();
    uint8_t dur = 0;

    #ifdef DEBUG
      if (dbg.should_stop(cycle, cmd, cpu.reg_pc)) {
        for (;;) {
          if (dbg.prompt()) break;
        }
      }
    #endif

    LOG_DEBUG(printf("CMD 0x%.2x @ 0x%.2x (%d) CYCLE %lu\n", cmd, cpu.reg_pc - 1, cpu.reg_pc - 1, cycle));

    if (cmd == 0x00) { // NOP | 1  4 | - - - -
      dur = 4;
    }
    else if (cmd == 0x01) { // LD BC,d16 | 3  12 | - - - -
      cpu.set_bc(read_next_hl());
      dur = 12;
    }
    else if (cmd == 0x02) { // LD (BC),A | 1  8 | - - - -
      set_mem(cpu.bc(), cpu.reg_a);
      dur = 8;
    }
    else if (cmd == 0x03) { // INC BC | 1  8 | - - - -
      cpu.inc_bc();
      dur = 8;
    }
    else if (cmd == 0x04) { // INC B | 1  4 | Z 0 H -
      op_inc(&cpu.reg_b);
      dur = 4;
    }
    else if (cmd == 0x05) { // DEC B | 1  4 | Z 1 H -
      op_dec(&cpu.reg_b);
      dur = 4;
    }
    else if (cmd == 0x06) { // LD B,d8 | 2  8 | - - - -
      cpu.reg_b = read_next();
      dur = 8;
    }
    else if (cmd == 0x07) { // RLCA | 1  4 | 0 0 0 C
      set_carry_flag(ISBITN(cpu.reg_a, 7));
      cpu.reg_a = rotate_left(cpu.reg_a);
      set_substract_flag(false);
      set_half_carry_flag(false);
      set_zero_flag(false);
      dur = 4;
    }
    else if (cmd == 0x08) { // LD (a16),SP | 3  20 | - - - -
      set_mem(read_next_hl(), cpu.reg_sp);
      dur = 20;
    }
    // else if (cmd == 0x09) { // ADD HL,BC | 1  8 | - 0 H C
    // }
    else if (cmd == 0x0A) { // LD A,(BC) | 1  8 | - - - -
      cpu.reg_a = get_mem(cpu.bc());
      dur = 8;
    }
    else if (cmd == 0x0B) { // DEC BC | 1  8 | - - - -
      cpu.dec_bc();
      dur = 8;
    }
    else if (cmd == 0x0C) { // INC C | 1  4 | Z 0 H -
      op_inc(&cpu.reg_c);
      dur = 4;
    }
    else if (cmd == 0x0D) { // DEC C | 1  4 | Z 1 H -
      op_dec(&cpu.reg_c);
      dur = 4;
    }
    else if (cmd == 0x0E) { // LD C,d8 | 2  8 | - - - -
      cpu.reg_c = read_next();
      dur = 8;
    }
    // else if (cmd == 0x0F) { // RRCA | 1  4 | 0 0 0 C
    // }
    // else if (cmd == 0x10) { // STOP 0 | 2  4 | - - - -
    // }
    else if (cmd == 0x11) { // LD DE,d16 | 3  12 | - - - -
      cpu.set_de(read_next_hl());
      dur = 12;
    }
    else if (cmd == 0x12) { // LD (DE),A | 1  8 | - - - -
      set_mem(cpu.de(), cpu.reg_a);
      dur = 8;
    }
    else if (cmd == 0x13) { // INC DE | 1  8 | - - - -
      cpu.inc_de();
      dur = 8;
    }
    else if (cmd == 0x14) { // INC D | 1  4 | Z 0 H -
      op_inc(&cpu.reg_d);
      dur = 4;
    }
    else if (cmd == 0x15) { // DEC D | 1  4 | Z 1 H -
      op_dec(&cpu.reg_d);
      dur = 4;
    }
    else if (cmd == 0x16) { // LD D,d8 | 2  8 | - - - -
      cpu.reg_d = read_next();
      dur = 8;
    }
    else if (cmd == 0x17) { // RLA | 1  4 | 0 0 0 C
      set_carry_flag(ISBITN(cpu.reg_a, 7));

      cpu.reg_a = rotate_left(cpu.reg_a);
      set_zero_flag(cpu.reg_a == 0);
      set_substract_flag(false);
      set_half_carry_flag(false);
      dur = 4;
    }
    // else if (cmd == 0x18) { // JR r8 | 2  12 | - - - -
    // }
    // else if (cmd == 0x19) { // ADD HL,DE | 1  8 | - 0 H C
    // }
    else if (cmd == 0x1A) { // LD A,(DE) | 1  8 | - - - -
      cpu.reg_a = get_mem(cpu.de());
      dur = 8;
    }
    else if (cmd == 0x1B) { // DEC DE | 1  8 | - - - -
      cpu.dec_de();
      dur = 8;
    }
    else if (cmd == 0x1C) { // INC E | 1  4 | Z 0 H -
      op_inc(&cpu.reg_e);
      dur = 4;
    }
    else if (cmd == 0x1D) { // DEC E | 1  4 | Z 1 H -
      op_dec(&cpu.reg_e);
      dur = 4;
    }
    else if (cmd == 0x1E) { // LD E,d8 | 2  8 | - - - -
      cpu.reg_e = read_next();
      dur = 8;
    }
    // else if (cmd == 0x1F) { // RRA | 1  4 | 0 0 0 C
    // }
    else if (cmd == 0x20) { // JR NZ,r8 | 2  12/8 | - - - -
      char offset = (char) read_next();
      dur = 8;

      if (!ISBITN(cpu.reg_f, BITFZ)) {
        dur = 12;
        cpu.reg_pc += offset;
      }
    }
    else if (cmd == 0x21) { // LD HL,d16 | 3  12 | - - - -
      cpu.reg_l = read_next();
      cpu.reg_h = read_next();
      dur = 12;
    }
    else if (cmd == 0x22) { // LD (HL+),A | 1  8 | - - - -
      set_mem(cpu.hl(), cpu.reg_a);
      cpu.inc_hl();
      dur = 8;
    }
    else if (cmd == 0x23) { // INC HL | 1  8 | - - - -
      cpu.inc_hl();
      dur = 8;
    }
    else if (cmd == 0x24) { // INC H | 1  4 | Z 0 H -
      op_inc(&cpu.reg_h);
      dur = 4;
    }
    else if (cmd == 0x25) { // DEC H | 1  4 | Z 1 H -
      op_dec(&cpu.reg_h);
      dur = 4;
    }
    else if (cmd == 0x26) { // LD H,d8 | 2  8 | - - - -
      cpu.reg_h = read_next();
      dur = 8;
    }
    // else if (cmd == 0x27) { // DAA | 1  4 | Z - 0 C
    // }
    else if (cmd == 0x28) { // JR Z,r8 | 2  12/8 | - - - -
      dur = 8;
      char offset = (char) read_next();

      if (ISBITN(cpu.reg_f, BITFZ)) {
        dur = 12;
        cpu.reg_pc += offset;
      }
    }
    // else if (cmd == 0x29) { // ADD HL,HL | 1  8 | - 0 H C
    // }
    else if (cmd == 0x2A) { // LD A,(HL+) | 1  8 | - - - -
      cpu.reg_a = get_mem(cpu.hl());
      cpu.inc_hl();
      dur = 8;
    }
    else if (cmd == 0x2B) { // DEC HL | 1  8 | - - - -
      cpu.dec_hl();
      dur = 8;
    }
    else if (cmd == 0x2C) { // INC L | 1  4 | Z 0 H -
      op_inc(&cpu.reg_l);
      dur = 4;
    }
    else if (cmd == 0x2D) { // DEC L | 1  4 | Z 1 H -
      op_dec(&cpu.reg_l);
      dur = 4;
    }
    else if (cmd == 0x2E) { // LD L,d8 | 2  8 | - - - -
      cpu.reg_l = read_next();
      dur = 8;
    }
    // else if (cmd == 0x2F) { // CPL | 1  4 | - 1 1 -
    // }
    // else if (cmd == 0x30) { // JR NC,r8 | 2  12/8 | - - - -
    // }
    else if (cmd == 0x31) { // LD SP,d16 | 3  12 | - - - -
      cpu.reg_sp = read_next_hl();
      dur = 12;
    }
    else if (cmd == 0x32) { // LD (HL-),A | 1  8 | - - - -
      set_mem(cpu.hl(), cpu.reg_a);
      cpu.dec_hl();
      dur = 8;
    }
    // else if (cmd == 0x33) { // INC SP | 1  8 | - - - -
    // }
    // else if (cmd == 0x34) { // INC (HL) | 1  12 | Z 0 H -
    // }
    // else if (cmd == 0x35) { // DEC (HL) | 1  12 | Z 1 H -
    // }
    else if (cmd == 0x36) { // LD (HL),d8 | 2  12 | - - - -
      set_mem(cpu.hl(), read_next());
      dur = 12;
    }
    // else if (cmd == 0x37) { // SCF | 1  4 | - 0 0 1
    // }
    // else if (cmd == 0x38) { // JR C,r8 | 2  12/8 | - - - -
    // }
    // else if (cmd == 0x39) { // ADD HL,SP | 1  8 | - 0 H C
    // }
    else if (cmd == 0x3A) { // LD A,(HL-) | 1  8 | - - - -
      cpu.reg_a = get_mem(cpu.hl());
      cpu.inc_hl();
      dur = 8;
    }
    // else if (cmd == 0x3B) { // DEC SP | 1  8 | - - - -
    // }
    else if (cmd == 0x3C) { // INC A | 1  4 | Z 0 H -
      op_inc(&cpu.reg_a);
      dur = 4;
    }
    else if (cmd == 0x3D) { // DEC A | 1  4 | Z 1 H -
      op_dec(&cpu.reg_a);
      dur = 4;
    }
    else if (cmd == 0x3E) { // LD A,d8 | 2  8 | - - - -
      cpu.reg_a = read_next();
      dur = 8;
    }
    // else if (cmd == 0x3F) { // CCF | 1  4 | - 0 0 C
    // }
    else if (cmd == 0x40) { // LD B,B | 1  4 | - - - -
      cpu.reg_b = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x41) { // LD B,C | 1  4 | - - - -
      cpu.reg_b = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x42) { // LD B,D | 1  4 | - - - -
      cpu.reg_b = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x43) { // LD B,E | 1  4 | - - - -
      cpu.reg_b = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x44) { // LD B,H | 1  4 | - - - -
      cpu.reg_b = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x45) { // LD B,L | 1  4 | - - - -
      cpu.reg_b = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x46) { // LD B,(HL) | 1  8 | - - - -
      cpu.reg_b = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x47) { // LD B,A | 1  4 | - - - -
      cpu.reg_b = cpu.reg_a;
      dur = 4;
    }
    else if (cmd == 0x48) { // LD C,B | 1  4 | - - - -
      cpu.reg_c = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x49) { // LD C,C | 1  4 | - - - -
      cpu.reg_c = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x4A) { // LD C,D | 1  4 | - - - -
      cpu.reg_c = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x4B) { // LD C,E | 1  4 | - - - -
      cpu.reg_c = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x4C) { // LD C,H | 1  4 | - - - -
      cpu.reg_c = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x4D) { // LD C,L | 1  4 | - - - -
      cpu.reg_c = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x4E) { // LD C,(HL) | 1  8 | - - - -
      cpu.reg_c = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x4F) { // LD C,A | 1  4 | - - - -
      cpu.reg_c = cpu.reg_a;
      dur = 4;
    }
    else if (cmd == 0x50) { // LD D,B | 1  4 | - - - -
      cpu.reg_d = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x51) { // LD D,C | 1  4 | - - - -
      cpu.reg_d = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x52) { // LD D,D | 1  4 | - - - -
      cpu.reg_d = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x53) { // LD D,E | 1  4 | - - - -
      cpu.reg_d = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x54) { // LD D,H | 1  4 | - - - -
      cpu.reg_d = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x55) { // LD D,L | 1  4 | - - - -
      cpu.reg_d = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x56) { // LD D,(HL) | 1  8 | - - - -
      cpu.reg_d = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x57) { // LD D,A | 1  4 | - - - -
      cpu.reg_d = cpu.reg_a;
      dur = 4;
    }
    else if (cmd == 0x58) { // LD E,B | 1  4 | - - - -
      cpu.reg_e = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x59) { // LD E,C | 1  4 | - - - -
      cpu.reg_e = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x5A) { // LD E,D | 1  4 | - - - -
      cpu.reg_e = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x5B) { // LD E,E | 1  4 | - - - -
      cpu.reg_e = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x5C) { // LD E,H | 1  4 | - - - -
      cpu.reg_e = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x5D) { // LD E,L | 1  4 | - - - -
      cpu.reg_e = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x5E) { // LD E,(HL) | 1  8 | - - - -
      cpu.reg_e = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x5F) { // LD E,A | 1  4 | - - - -
      cpu.reg_e = cpu.reg_a;
      dur = 4;
    }
    else if (cmd == 0x60) { // LD H,B | 1  4 | - - - -
      cpu.reg_h = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x61) { // LD H,C | 1  4 | - - - -
      cpu.reg_h = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x62) { // LD H,D | 1  4 | - - - -
      cpu.reg_h = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x63) { // LD H,E | 1  4 | - - - -
      cpu.reg_h = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x64) { // LD H,H | 1  4 | - - - -
      cpu.reg_h = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x65) { // LD H,L | 1  4 | - - - -
      cpu.reg_h = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x66) { // LD H,(HL) | 1  8 | - - - -
      cpu.reg_h = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x67) { // LD H,A | 1  4 | - - - -
      cpu.reg_h = cpu.reg_a;
      dur = 4;
    }
    else if (cmd == 0x68) { // LD L,B | 1  4 | - - - -
      cpu.reg_l = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x69) { // LD L,C | 1  4 | - - - -
      cpu.reg_l = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x6A) { // LD L,D | 1  4 | - - - -
      cpu.reg_l = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x6B) { // LD L,E | 1  4 | - - - -
      cpu.reg_l = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x6C) { // LD L,H | 1  4 | - - - -
      cpu.reg_l = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x6D) { // LD L,L | 1  4 | - - - -
      cpu.reg_l = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x6E) { // LD L,(HL) | 1  8 | - - - -
      cpu.reg_l = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x6F) { // LD L,A | 1  4 | - - - -
      cpu.reg_l = cpu.reg_a;
      dur = 4;
    }
    // else if (cmd == 0x70) { // LD (HL),B | 1  8 | - - - -
    // }
    // else if (cmd == 0x71) { // LD (HL),C | 1  8 | - - - -
    // }
    // else if (cmd == 0x72) { // LD (HL),D | 1  8 | - - - -
    // }
    // else if (cmd == 0x73) { // LD (HL),E | 1  8 | - - - -
    // }
    // else if (cmd == 0x74) { // LD (HL),H | 1  8 | - - - -
    // }
    // else if (cmd == 0x75) { // LD (HL),L | 1  8 | - - - -
    // }
    // else if (cmd == 0x76) { // HALT | 1  4 | - - - -
    // }
    else if (cmd == 0x77) { // LD (HL),A | 1  8 | - - - -
      set_mem(cpu.hl(), cpu.reg_a);
      dur = 8;
    }
    else if (cmd == 0x78) { // LD A,B | 1  4 | - - - -
      cpu.reg_a = cpu.reg_b;
      dur = 4;
    }
    else if (cmd == 0x79) { // LD A,C | 1  4 | - - - -
      cpu.reg_a = cpu.reg_c;
      dur = 4;
    }
    else if (cmd == 0x7A) { // LD A,D | 1  4 | - - - -
      cpu.reg_a = cpu.reg_d;
      dur = 4;
    }
    else if (cmd == 0x7B) { // LD A,E | 1  4 | - - - -
      cpu.reg_a = cpu.reg_e;
      dur = 4;
    }
    else if (cmd == 0x7C) { // LD A,H | 1  4 | - - - -
      cpu.reg_a = cpu.reg_h;
      dur = 4;
    }
    else if (cmd == 0x7D) { // LD A,L | 1  4 | - - - -
      cpu.reg_a = cpu.reg_l;
      dur = 4;
    }
    else if (cmd == 0x7E) { // LD A,(HL) | 1  8 | - - - -
      cpu.reg_a = get_mem(cpu.hl());
      dur = 8;
    }
    else if (cmd == 0x7F) { // LD A,A | 1  4 | - - - -
      cpu.reg_a = cpu.reg_a;
      dur = 4;
    }
    // else if (cmd == 0x80) { // ADD A,B | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x81) { // ADD A,C | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x82) { // ADD A,D | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x83) { // ADD A,E | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x84) { // ADD A,H | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x85) { // ADD A,L | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x86) { // ADD A,(HL) | 1  8 | Z 0 H C
    // }
    // else if (cmd == 0x87) { // ADD A,A | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x88) { // ADC A,B | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x89) { // ADC A,C | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x8A) { // ADC A,D | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x8B) { // ADC A,E | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x8C) { // ADC A,H | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x8D) { // ADC A,L | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x8E) { // ADC A,(HL) | 1  8 | Z 0 H C
    // }
    // else if (cmd == 0x8F) { // ADC A,A | 1  4 | Z 0 H C
    // }
    // else if (cmd == 0x90) { // SUB B | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x91) { // SUB C | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x92) { // SUB D | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x93) { // SUB E | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x94) { // SUB H | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x95) { // SUB L | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x96) { // SUB (HL) | 1  8 | Z 1 H C
    // }
    // else if (cmd == 0x97) { // SUB A | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x98) { // SBC A,B | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x99) { // SBC A,C | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x9A) { // SBC A,D | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x9B) { // SBC A,E | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x9C) { // SBC A,H | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x9D) { // SBC A,L | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0x9E) { // SBC A,(HL) | 1  8 | Z 1 H C
    // }
    // else if (cmd == 0x9F) { // SBC A,A | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xA0) { // AND B | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA1) { // AND C | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA2) { // AND D | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA3) { // AND E | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA4) { // AND H | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA5) { // AND L | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA6) { // AND (HL) | 1  8 | Z 0 1 0
    // }
    // else if (cmd == 0xA7) { // AND A | 1  4 | Z 0 1 0
    // }
    // else if (cmd == 0xA8) { // XOR B | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xA9) { // XOR C | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xAA) { // XOR D | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xAB) { // XOR E | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xAC) { // XOR H | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xAD) { // XOR L | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xAE) { // XOR (HL) | 1  8 | Z 0 0 0
    // }
    else if (cmd == 0xAF) { // XOR A | 1  4 | Z 0 0 0
      cpu.reg_a ^= cpu.reg_a;
      set_zero_flag(cpu.reg_a == 0);
      set_substract_flag(false);
      set_half_carry_flag(false);
      set_carry_flag(false);

      dur = 4;
    }
    // else if (cmd == 0xB0) { // OR B | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB1) { // OR C | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB2) { // OR D | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB3) { // OR E | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB4) { // OR H | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB5) { // OR L | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB6) { // OR (HL) | 1  8 | Z 0 0 0
    // }
    // else if (cmd == 0xB7) { // OR A | 1  4 | Z 0 0 0
    // }
    // else if (cmd == 0xB8) { // CP B | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xB9) { // CP C | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xBA) { // CP D | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xBB) { // CP E | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xBC) { // CP H | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xBD) { // CP L | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xBE) { // CP (HL) | 1  8 | Z 1 H C
    // }
    // else if (cmd == 0xBF) { // CP A | 1  4 | Z 1 H C
    // }
    // else if (cmd == 0xC0) { // RET NZ | 1  20/8 | - - - -
    // }
    else if (cmd == 0xC1) { // POP BC | 1  12 | - - - -
      dur = 12;
      cpu.set_bc(pop_from_stack_d16());
    }
    // else if (cmd == 0xC2) { // JP NZ,a16 | 3  16/12 | - - - -
    // }
    // else if (cmd == 0xC3) { // JP a16 | 3  16 | - - - -
    // }
    // else if (cmd == 0xC4) { // CALL NZ,a16 | 3  24/12 | - - - -
    // }
    else if (cmd == 0xC5) { // PUSH BC | 1  16 | - - - -
      push_to_stack_d16(cpu.bc());
      dur = 16;
    }
    // else if (cmd == 0xC6) { // ADD A,d8 | 2  8 | Z 0 H C
    // }
    // else if (cmd == 0xC7) { // RST 00H | 1  16 | - - - -
    // }
    // else if (cmd == 0xC8) { // RET Z | 1  20/8 | - - - -
    // }
    // else if (cmd == 0xC9) { // RET | 1  16 | - - - -
    // }
    // else if (cmd == 0xCA) { // JP Z,a16 | 3  16/12 | - - - -
    // }
    else if (cmd == 0xCB) { // PREFIX CB | 1  4 | - - - -
      dur = 4;

      cmd = read_next();

      if (cmd == 0x00) { // RLC B | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_b, &dur);
      }
      else if (cmd == 0x01) { // RLC C | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_c, &dur);
      }
      else if (cmd == 0x02) { // RLC D | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_d, &dur);
      }
      else if (cmd == 0x03) { // RLC E | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_e, &dur);
      }
      else if (cmd == 0x04) { // RLC H | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_h, &dur);
      }
      else if (cmd == 0x05) { // RLC L | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_l, &dur);
      }
      else if (cmd == 0x06) { // RLC (HL) | 2  16 | Z 0 0 C
        op_rlc_n(get_mem_ptr(cpu.hl()), &dur);
        dur = 16;
      }
      else if (cmd == 0x07) { // RLC A | 2  8 | Z 0 0 C
        op_rlc_n(&cpu.reg_a, &dur);
      }
      else if (cmd == 0x08) { // RRC B | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_b, &dur);
      }
      else if (cmd == 0x09) { // RRC C | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_c, &dur);
      }
      else if (cmd == 0x0a) { // RRC D | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_d, &dur);
      }
      else if (cmd == 0x0b) { // RRC E | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_e, &dur);
      }
      else if (cmd == 0x0c) { // RRC H | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_h, &dur);
      }
      else if (cmd == 0x0d) { // RRC L | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_l, &dur);
      }
      else if (cmd == 0x0e) { // RRC (HL) | 2  16 | Z 0 0 C
        op_rrc_n(get_mem_ptr(cpu.hl()), &dur);
        dur = 16;
      }
      else if (cmd == 0x0f) { // RRC A | 2  8 | Z 0 0 C
        op_rrc_n(&cpu.reg_a, &dur);
      }
      else if (cmd == 0x10) { // RL B | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_b, &dur);
      }
      else if (cmd == 0x11) { // RL C | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_c, &dur);
      }
      else if (cmd == 0x12) { // RL D | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_d, &dur);
      }
      else if (cmd == 0x13) { // RL E | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_e, &dur);
      }
      else if (cmd == 0x14) { // RL H | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_h, &dur);
      }
      else if (cmd == 0x15) { // RL L | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_l, &dur);
      }
      else if (cmd == 0x16) { // RL (HL) | 2  16 | Z 0 0 C
        op_rl_n(get_mem_ptr(cpu.hl()), &dur);
        dur = 16;
      }
      else if (cmd == 0x17) { // RL A | 2  8 | Z 0 0 C
        op_rl_n(&cpu.reg_a, &dur);
      }
      else if (cmd == 0x18) { // RR B | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_b, &dur);
      }
      else if (cmd == 0x19) { // RR C | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_c, &dur);
      }
      else if (cmd == 0x1a) { // RR D | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_d, &dur);
      }
      else if (cmd == 0x1b) { // RR E | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_e, &dur);
      }
      else if (cmd == 0x1c) { // RR H | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_h, &dur);
      }
      else if (cmd == 0x1d) { // RR L | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_l, &dur);
      }
      else if (cmd == 0x1e) { // RR (HL) | 2  16 | Z 0 0 C
        op_rr_n(get_mem_ptr(cpu.hl()), &dur);
        dur = 16;
      }
      else if (cmd == 0x1f) { // RR A | 2  8 | Z 0 0 C
        op_rr_n(&cpu.reg_a, &dur);
      }
      // else if (cmd == 0x20) { // SLA B | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x21) { // SLA C | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x22) { // SLA D | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x23) { // SLA E | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x24) { // SLA H | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x25) { // SLA L | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x26) { // SLA (HL) | 2  16 | Z 0 0 C
      // }
      // else if (cmd == 0x27) { // SLA A | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x28) { // SRA B | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x29) { // SRA C | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x2a) { // SRA D | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x2b) { // SRA E | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x2c) { // SRA H | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x2d) { // SRA L | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x2e) { // SRA (HL) | 2  16 | Z 0 0 0
      // }
      // else if (cmd == 0x2f) { // SRA A | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x30) { // SWAP B | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x31) { // SWAP C | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x32) { // SWAP D | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x33) { // SWAP E | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x34) { // SWAP H | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x35) { // SWAP L | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x36) { // SWAP (HL) | 2  16 | Z 0 0 0
      // }
      // else if (cmd == 0x37) { // SWAP A | 2  8 | Z 0 0 0
      // }
      // else if (cmd == 0x38) { // SRL B | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x39) { // SRL C | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x3a) { // SRL D | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x3b) { // SRL E | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x3c) { // SRL H | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x3d) { // SRL L | 2  8 | Z 0 0 C
      // }
      // else if (cmd == 0x3e) { // SRL (HL) | 2  16 | Z 0 0 C
      // }
      // else if (cmd == 0x3f) { // SRL A | 2  8 | Z 0 0 C
      // }
      else if (cmd == 0x40) { // BIT 0,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 0);
      }
      else if (cmd == 0x41) { // BIT 0,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 0);
      }
      else if (cmd == 0x42) { // BIT 0,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 0);
      }
      else if (cmd == 0x43) { // BIT 0,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 0);
      }
      else if (cmd == 0x44) { // BIT 0,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 0);
      }
      else if (cmd == 0x45) { // BIT 0,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 0);
      }
      else if (cmd == 0x46) { // BIT 0,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 0);
      }
      else if (cmd == 0x47) { // BIT 0,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 0);
      }
      else if (cmd == 0x48) { // BIT 1,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 1);
      }
      else if (cmd == 0x49) { // BIT 1,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 1);
      }
      else if (cmd == 0x4a) { // BIT 1,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 1);
      }
      else if (cmd == 0x4b) { // BIT 1,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 1);
      }
      else if (cmd == 0x4c) { // BIT 1,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 1);
      }
      else if (cmd == 0x4d) { // BIT 1,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 1);
      }
      else if (cmd == 0x4e) { // BIT 1,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 1);
      }
      else if (cmd == 0x4f) { // BIT 1,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 1);
      }
      else if (cmd == 0x50) { // BIT 2,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 2);
      }
      else if (cmd == 0x51) { // BIT 2,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 2);
      }
      else if (cmd == 0x52) { // BIT 2,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 2);
      }
      else if (cmd == 0x53) { // BIT 2,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 2);
      }
      else if (cmd == 0x54) { // BIT 2,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 2);
      }
      else if (cmd == 0x55) { // BIT 2,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 2);
      }
      else if (cmd == 0x56) { // BIT 2,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 2);
      }
      else if (cmd == 0x57) { // BIT 2,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 2);
      }
      else if (cmd == 0x58) { // BIT 3,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 3);
      }
      else if (cmd == 0x59) { // BIT 3,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 3);
      }
      else if (cmd == 0x5a) { // BIT 3,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 3);
      }
      else if (cmd == 0x5b) { // BIT 3,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 3);
      }
      else if (cmd == 0x5c) { // BIT 3,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 3);
      }
      else if (cmd == 0x5d) { // BIT 3,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 3);
      }
      else if (cmd == 0x5e) { // BIT 3,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 3);
      }
      else if (cmd == 0x5f) { // BIT 3,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 3);
      }
      else if (cmd == 0x60) { // BIT 4,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 4);
      }
      else if (cmd == 0x61) { // BIT 4,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 4);
      }
      else if (cmd == 0x62) { // BIT 4,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 4);
      }
      else if (cmd == 0x63) { // BIT 4,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 4);
      }
      else if (cmd == 0x64) { // BIT 4,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 4);
      }
      else if (cmd == 0x65) { // BIT 4,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 4);
      }
      else if (cmd == 0x66) { // BIT 4,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 4);
      }
      else if (cmd == 0x67) { // BIT 4,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 4);
      }
      else if (cmd == 0x68) { // BIT 5,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 5);
      }
      else if (cmd == 0x69) { // BIT 5,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 5);
      }
      else if (cmd == 0x6a) { // BIT 5,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 5);
      }
      else if (cmd == 0x6b) { // BIT 5,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 5);
      }
      else if (cmd == 0x6c) { // BIT 5,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 5);
      }
      else if (cmd == 0x6d) { // BIT 5,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 5);
      }
      else if (cmd == 0x6e) { // BIT 5,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 5);
      }
      else if (cmd == 0x6f) { // BIT 5,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 5);
      }
      else if (cmd == 0x70) { // BIT 6,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 6);
      }
      else if (cmd == 0x71) { // BIT 6,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 6);
      }
      else if (cmd == 0x72) { // BIT 6,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 6);
      }
      else if (cmd == 0x73) { // BIT 6,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 6);
      }
      else if (cmd == 0x74) { // BIT 6,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 6);
      }
      else if (cmd == 0x75) { // BIT 6,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 6);
      }
      else if (cmd == 0x76) { // BIT 6,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 6);
      }
      else if (cmd == 0x77) { // BIT 6,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 6);
      }
      else if (cmd == 0x78) { // BIT 7,B | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_b, &dur, 7);
      }
      else if (cmd == 0x79) { // BIT 7,C | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_c, &dur, 7);
      }
      else if (cmd == 0x7a) { // BIT 7,D | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_d, &dur, 7);
      }
      else if (cmd == 0x7b) { // BIT 7,E | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_e, &dur, 7);
      }
      else if (cmd == 0x7c) { // BIT 7,H | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_h, &dur, 7);
      }
      else if (cmd == 0x7d) { // BIT 7,L | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_l, &dur, 7);
      }
      else if (cmd == 0x7e) { // BIT 7,(HL) | 2  16 | Z 0 1 -
        op_bit_n_d8(get_mem(cpu.hl()), &dur, 7);
      }
      else if (cmd == 0x7f) { // BIT 7,A | 2  8 | Z 0 1 -
        op_bit_n_d8(cpu.reg_a, &dur, 7);
      }
      // else if (cmd == 0x80) { // RES 0,B | 2  8 | - - - -
      // }
      // else if (cmd == 0x81) { // RES 0,C | 2  8 | - - - -
      // }
      // else if (cmd == 0x82) { // RES 0,D | 2  8 | - - - -
      // }
      // else if (cmd == 0x83) { // RES 0,E | 2  8 | - - - -
      // }
      // else if (cmd == 0x84) { // RES 0,H | 2  8 | - - - -
      // }
      // else if (cmd == 0x85) { // RES 0,L | 2  8 | - - - -
      // }
      // else if (cmd == 0x86) { // RES 0,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0x87) { // RES 0,A | 2  8 | - - - -
      // }
      // else if (cmd == 0x88) { // RES 1,B | 2  8 | - - - -
      // }
      // else if (cmd == 0x89) { // RES 1,C | 2  8 | - - - -
      // }
      // else if (cmd == 0x8a) { // RES 1,D | 2  8 | - - - -
      // }
      // else if (cmd == 0x8b) { // RES 1,E | 2  8 | - - - -
      // }
      // else if (cmd == 0x8c) { // RES 1,H | 2  8 | - - - -
      // }
      // else if (cmd == 0x8d) { // RES 1,L | 2  8 | - - - -
      // }
      // else if (cmd == 0x8e) { // RES 1,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0x8f) { // RES 1,A | 2  8 | - - - -
      // }
      // else if (cmd == 0x90) { // RES 2,B | 2  8 | - - - -
      // }
      // else if (cmd == 0x91) { // RES 2,C | 2  8 | - - - -
      // }
      // else if (cmd == 0x92) { // RES 2,D | 2  8 | - - - -
      // }
      // else if (cmd == 0x93) { // RES 2,E | 2  8 | - - - -
      // }
      // else if (cmd == 0x94) { // RES 2,H | 2  8 | - - - -
      // }
      // else if (cmd == 0x95) { // RES 2,L | 2  8 | - - - -
      // }
      // else if (cmd == 0x96) { // RES 2,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0x97) { // RES 2,A | 2  8 | - - - -
      // }
      // else if (cmd == 0x98) { // RES 3,B | 2  8 | - - - -
      // }
      // else if (cmd == 0x99) { // RES 3,C | 2  8 | - - - -
      // }
      // else if (cmd == 0x9a) { // RES 3,D | 2  8 | - - - -
      // }
      // else if (cmd == 0x9b) { // RES 3,E | 2  8 | - - - -
      // }
      // else if (cmd == 0x9c) { // RES 3,H | 2  8 | - - - -
      // }
      // else if (cmd == 0x9d) { // RES 3,L | 2  8 | - - - -
      // }
      // else if (cmd == 0x9e) { // RES 3,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0x9f) { // RES 3,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xa0) { // RES 4,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xa1) { // RES 4,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xa2) { // RES 4,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xa3) { // RES 4,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xa4) { // RES 4,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xa5) { // RES 4,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xa6) { // RES 4,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xa7) { // RES 4,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xa8) { // RES 5,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xa9) { // RES 5,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xaa) { // RES 5,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xab) { // RES 5,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xac) { // RES 5,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xad) { // RES 5,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xae) { // RES 5,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xaf) { // RES 5,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xb0) { // RES 6,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xb1) { // RES 6,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xb2) { // RES 6,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xb3) { // RES 6,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xb4) { // RES 6,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xb5) { // RES 6,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xb6) { // RES 6,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xb7) { // RES 6,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xb8) { // RES 7,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xb9) { // RES 7,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xba) { // RES 7,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xbb) { // RES 7,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xbc) { // RES 7,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xbd) { // RES 7,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xbe) { // RES 7,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xbf) { // RES 7,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xc0) { // SET 0,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xc1) { // SET 0,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xc2) { // SET 0,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xc3) { // SET 0,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xc4) { // SET 0,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xc5) { // SET 0,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xc6) { // SET 0,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xc7) { // SET 0,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xc8) { // SET 1,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xc9) { // SET 1,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xca) { // SET 1,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xcb) { // SET 1,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xcc) { // SET 1,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xcd) { // SET 1,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xce) { // SET 1,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xcf) { // SET 1,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xd0) { // SET 2,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xd1) { // SET 2,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xd2) { // SET 2,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xd3) { // SET 2,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xd4) { // SET 2,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xd5) { // SET 2,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xd6) { // SET 2,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xd7) { // SET 2,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xd8) { // SET 3,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xd9) { // SET 3,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xda) { // SET 3,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xdb) { // SET 3,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xdc) { // SET 3,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xdd) { // SET 3,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xde) { // SET 3,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xdf) { // SET 3,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xe0) { // SET 4,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xe1) { // SET 4,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xe2) { // SET 4,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xe3) { // SET 4,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xe4) { // SET 4,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xe5) { // SET 4,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xe6) { // SET 4,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xe7) { // SET 4,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xe8) { // SET 5,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xe9) { // SET 5,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xea) { // SET 5,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xeb) { // SET 5,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xec) { // SET 5,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xed) { // SET 5,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xee) { // SET 5,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xef) { // SET 5,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xf0) { // SET 6,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xf1) { // SET 6,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xf2) { // SET 6,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xf3) { // SET 6,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xf4) { // SET 6,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xf5) { // SET 6,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xf6) { // SET 6,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xf7) { // SET 6,A | 2  8 | - - - -
      // }
      // else if (cmd == 0xf8) { // SET 7,B | 2  8 | - - - -
      // }
      // else if (cmd == 0xf9) { // SET 7,C | 2  8 | - - - -
      // }
      // else if (cmd == 0xfa) { // SET 7,D | 2  8 | - - - -
      // }
      // else if (cmd == 0xfb) { // SET 7,E | 2  8 | - - - -
      // }
      // else if (cmd == 0xfc) { // SET 7,H | 2  8 | - - - -
      // }
      // else if (cmd == 0xfd) { // SET 7,L | 2  8 | - - - -
      // }
      // else if (cmd == 0xfe) { // SET 7,(HL) | 2  16 | - - - -
      // }
      // else if (cmd == 0xff) { // SET 7,A | 2  8 | - - - -
      // }
      else {
        ERR(printf("Unknown 0xCB command: 0x%.2x @ 0x%.2x (%d)", cmd, cpu.reg_pc - 1, (int) cpu.reg_pc - 1));
        break;
      }
    }
    // else if (cmd == 0xCC) { // CALL Z,a16 | 3  24/12 | - - - -
    // }
    else if (cmd == 0xCD) { // CALL a16 | 3  24 | - - - -
      push_to_stack_d16(cpu.reg_pc);
      uint16_t addr = read_next_hl();
      cpu.reg_pc = addr;
      dur = 24;
    }
    // else if (cmd == 0xCE) { // ADC A,d8 | 2  8 | Z 0 H C
    // }
    // else if (cmd == 0xCF) { // RST 08H | 1  16 | - - - -
    // }
    // else if (cmd == 0xD0) { // RET NC | 1  20/8 | - - - -
    // }
    // else if (cmd == 0xD1) { // POP DE | 1  12 | - - - -
    // }
    // else if (cmd == 0xD2) { // JP NC,a16 | 3  16/12 | - - - -
    // }
    // else if (cmd == 0xD4) { // CALL NC,a16 | 3  24/12 | - - - -
    // }
    // else if (cmd == 0xD5) { // PUSH DE | 1  16 | - - - -
    // }
    // else if (cmd == 0xD6) { // SUB d8 | 2  8 | Z 1 H C
    // }
    // else if (cmd == 0xD7) { // RST 10H | 1  16 | - - - -
    // }
    // else if (cmd == 0xD8) { // RET C | 1  20/8 | - - - -
    // }
    // else if (cmd == 0xD9) { // RETI | 1  16 | - - - -
    // }
    // else if (cmd == 0xDA) { // JP C,a16 | 3  16/12 | - - - -
    // }
    // else if (cmd == 0xDC) { // CALL C,a16 | 3  24/12 | - - - -
    // }
    // else if (cmd == 0xDE) { // SBC A,d8 | 2  8 | Z 1 H C
    // }
    // else if (cmd == 0xDF) { // RST 18H | 1  16 | - - - -
    // }
    else if (cmd == 0xE0) { // LDH (a8),A | 2  12 | - - - -
      dur = 12;
      uint8_t addr = 0xFF00 | read_next();
      set_mem(addr, cpu.reg_a);
    }
    // else if (cmd == 0xE1) { // POP HL | 1  12 | - - - -
    // }
    else if (cmd == 0xE2) { // LD (C),A | 2  8 | - - - -
      uint8_t addr = 0xFF00 | cpu.reg_c;
      set_mem(addr, cpu.reg_a);
      dur = 8;
    }
    // else if (cmd == 0xE5) { // PUSH HL | 1  16 | - - - -
    // }
    // else if (cmd == 0xE6) { // AND d8 | 2  8 | Z 0 1 0
    // }
    // else if (cmd == 0xE7) { // RST 20H | 1  16 | - - - -
    // }
    // else if (cmd == 0xE8) { // ADD SP,r8 | 2  16 | 0 0 H C
    // }
    // else if (cmd == 0xE9) { // JP (HL) | 1  4 | - - - -
    // }
    // else if (cmd == 0xEA) { // LD (a16),A | 3  16 | - - - -
    // }
    // else if (cmd == 0xEE) { // XOR d8 | 2  8 | Z 0 0 0
    // }
    // else if (cmd == 0xEF) { // RST 28H | 1  16 | - - - -
    // }
    // else if (cmd == 0xF0) { // LDH A,(a8) | 2  12 | - - - -
    // }
    // else if (cmd == 0xF1) { // POP AF | 1  12 | Z N H C
    // }
    // else if (cmd == 0xF2) { // LD A,(C) | 2  8 | - - - -
    // }
    // else if (cmd == 0xF3) { // DI | 1  4 | - - - -
    // }
    // else if (cmd == 0xF5) { // PUSH AF | 1  16 | - - - -
    // }
    // else if (cmd == 0xF6) { // OR d8 | 2  8 | Z 0 0 0
    // }
    // else if (cmd == 0xF7) { // RST 30H | 1  16 | - - - -
    // }
    // else if (cmd == 0xF8) { // LD HL,SP+r8 | 2  12 | 0 0 H C
    // }
    // else if (cmd == 0xF9) { // LD SP,HL | 1  8 | - - - -
    // }
    // else if (cmd == 0xFA) { // LD A,(a16) | 3  16 | - - - -
    // }
    // else if (cmd == 0xFB) { // EI | 1  4 | - - - -
    // }
    // else if (cmd == 0xFE) { // CP d8 | 2  8 | Z 1 H C
    // }
    // else if (cmd == 0xFF) { // RST 38H | 1  16 | - - - -
    // }
    else {
      ERR(printf("Unknown command: 0x%.2x @ 0x%.2x (%d)", cmd, cpu.reg_pc - 1, (int) cpu.reg_pc - 1));
      break;
    }

    LOG_INFO(cpu.dump_registers());
    LOG_NOTICE(cout << "Duration: " << (int) dur << endl);

    t += dur;

    // Divider register handler.
    if (t_div + dur >= 0x100) {
      mem[ADDR_DIV]++;
    }
    t_div += dur;

    // Timer counter handler.
    handle_timer_counter(dur);

    cycle++;
  }
}
