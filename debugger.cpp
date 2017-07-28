#include "debugger.h"
#include "util.h"
#include "defines.h"
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

Debugger::Debugger(uint8_t *_ptr_env_mem) :
  ptr_env_mem(_ptr_env_mem),
  cond_cycle_stop(0),
  cond_step_by_step(false),
  cond_step_counter(0)
{
  cout << "Debugger has been created\n";
}

bool Debugger::prompt() {
  string s;
  BOLD(cout << "DBG>> ");
  getline(cin, s);

  DebugCommand command = parse_command(s);
  uint16_t addr;
  uint8_t val;
  switch (command) {
    case Quit:
      exit(EXIT_SUCCESS);

    case Cycle:
      cond_cycle_stop = parse_param<size_t>(s, 1);
      cout << "Cycle stop set at " << cond_cycle_stop << endl;
      return false;

    case StepByStep:
      cond_step_by_step = !cond_step_by_step;
      cout << "Step by step is " << (cond_step_by_step ? "on" : "off") << endl;
      return false;

    case Step:
      cond_step_counter = parse_param<size_t>(s, 1);
      cout << "Step " << cond_step_counter << endl;
      return false;

    case Dump:
      cond_dump = true;
      return false;

    case MemRead:
      addr = parse_param<uint16_t>(s, 1, true);
      printf("M[0x%x] => 0b", addr);
      val = ptr_env_mem[addr];
      dump_bin(val);
      printf(" 0x%x %d\n", val, val);
      return false;

    default:
      return true;
  }
}

bool Debugger::should_stop(uint64_t cycle, uint8_t op, uint16_t pc) {
  if (cond_cycle_stop == cycle) {
    return true;
  }

  if (cond_step_by_step) {
    return true;
  }

  if (cond_step_counter > 0) {
    cond_step_counter--;
    if (cond_step_counter == 0) {
      return true;
    }
  }

  return false;
}

bool Debugger::should_dump() {
  if (cond_dump) {
    cond_dump = false;
    return true;
  }
  return false;
}

DebugCommand Debugger::parse_command(string in) {
  string command = parse_param<string>(in, 0);

  if (command == "q" || command == "quit" || command == "exit") {
    return Quit;
  } else if (command == "c" || command == "cycle") {
    return Cycle;
  } else if (command == "sbs") {
    return StepByStep;
  } else if (command == "s" || command == "step") {
    return Step;
  } else if (command == "d" || command == "dump") {
    return Dump;
  } else if (command == "m" || command == "mem") {
    return MemRead;
  } else {
    return Nop;
  }
}

template<typename T>
T Debugger::parse_param(string in, size_t idx, bool as_hex) {
  istringstream iss(in);

  for (int i = 0; i < idx; i++) {
    string cmd;
    iss >> cmd;
  }

  T out;
  if (as_hex) {
    iss >> hex >> out;
  } else {
    iss >> out;
  }
  return out;
}
