# Current status

- Branch: `simulator`.
- Last completed pass: `feat: extend blocker peeling workflows`.
- Current working-tree baseline at P4-P7/P9 close: 89/89 CTest cases
  passed; analog matrix 15/15 against `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: Q3 closed-loop drive/solve/readback support required for P8,
  followed by generated campaigns and candidate councils.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
- P4-P7/P9 direct Claude review: the first tool-enabled gate stalled; the exact
  staged-diff gate found a non-causal newline-count assumption and an untested
  intermediate `set_output()` direction state. Both were corrected with
  explicit CRLF and isolated direction witnesses before the 89/89 run.
