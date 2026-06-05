# Current status

- Branch: `simulator`.
- Last committed pass: `eef54f2 feat: model timed UART topology`.
- Current working-tree baseline: 45/45 CTest cases passed; analog matrix 14/14 against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: Q2 line-aware timed I2C transactions.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
