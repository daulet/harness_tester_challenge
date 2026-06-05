# Current status

- Branch: `simulator`.
- Last validated commit: `0f14a53 docs: catalog simulator-backed issues`.
- Baseline: 41/41 CTest cases passed on 2026-06-05; analog matrix 14/14 against
  `/opt/homebrew/bin/ngspice`.
- Active item: Q1 sanitizer witnesses for A03 and A05.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
