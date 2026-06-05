# Current status

- Branch: `simulator`.
- Last validated commit: `0f14a53 docs: catalog simulator-backed issues`.
- Baseline before Q1b: 41/41 CTest cases passed; analog matrix 14/14 against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: Q1 mutation controls and issue-catalog refresh.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
