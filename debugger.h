#pragma once

#include <string>

using namespace std;

enum DebugCommand {
  Nop,
  Cycle,
  Quit,
  StepByStep,
  Step,
};

class Debugger {
public:
  Debugger();
  bool prompt();
  bool should_stop(uint64_t, uint8_t, uint16_t);

private:
  DebugCommand parse_command(string);

  template<typename T>
  T parse_param(string, size_t);

  size_t cond_cycle_stop;
  bool   cond_step_by_step;
  size_t cond_step_counter;
};
