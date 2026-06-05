# Current status

- Existing digital suite passed before analog edits: 27/27 tests.
- ngspice 46_1 installed locally through Homebrew for boundary validation.
- Analog matrix passes 14/14 cases against `/opt/homebrew/bin/ngspice`.
- Post-change `cmake -S . -B build`, `cmake --build build`, and full CTest gate
  passed 41/41 tests on 2026-06-05.
