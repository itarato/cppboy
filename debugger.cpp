#include "debugger.h"
#include <string>
#include <iostream>

using namespace std;

Debugger::Debugger() {
  cout << "Debugger has been created\n";
}

void Debugger::prompt() {
  string s;
  cout << "DBG>> ";
  getline(cin, s);

  DebugCommand command = parse_command(s);
  switch (command) {
    case Quit:
      exit(EXIT_SUCCESS);



    default:
      break;
  }
}

bool Debugger::should_stop() {
  return true;
}

DebugCommand Debugger::parse_command(string in) {
  size_t space_pos = in.find_first_of(" ");

  string command;
  if (space_pos == string::npos) {
    command = in;
  } else {
    command = in.substr(0, space_pos);
  }

  if (command == "q" || command == "quit" || command == "exit") {
    return Quit;
  } else if (command == "c" || command == "cycle") {
    return Cycle;
  } else {
    return Nop;
  }
}
