#pragma once

#include <string>

using namespace std;

enum DebugCommand {
  Nop,
  Cycle,
  Quit,
};

class Debugger {
public:
  Debugger();
  void prompt();
  bool should_stop();

private:
  DebugCommand parse_command(string);
};
