# Current status

- Branch: `simulator`.
- Last committed pass: `4409fc2 feat: add deterministic simulator event queue`.
- Current working-tree baseline: 44/44 CTest cases passed; analog matrix 14/14 against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: Q2 button bounce and reset/power-on timing.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
