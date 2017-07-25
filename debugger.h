#pragma once

#include <string>

using namespace std;

enum DebugCommand {
  Nop,
  Cycle,
  Quit,
  StepByStep,
  Step,
  Dump,
  MemRead,
};

class Debugger {
public:
  Debugger(uint8_t *);
  bool prompt();
  bool should_stop(uint64_t, uint8_t, uint16_t);
  bool should_dump();

private:
  DebugCommand parse_command(string);

  uint8_t *ptr_env_mem;

  template<typename T>
  T parse_param(string, size_t);

  size_t cond_cycle_stop;
  bool   cond_step_by_step;
  size_t cond_step_counter;
  bool   cond_dump;
};
