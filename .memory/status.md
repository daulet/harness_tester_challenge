# Current status

- Branch: `simulator`.
- Last committed pass: `095bab8 feat: model SD write failures`.
- Current working-tree baseline: 55/55 CTest cases passed; analog matrix 14/14 against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: close Q2 finite Teensy UART RX capacity and overrun behavior.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
