# Current status

- Branch: `simulator`.
- Last completed pass: `feat: add blocker peeling variants`.
- Current working-tree baseline at P0-P3 close: 71/71 CTest cases
  passed; analog matrix 15/15 against `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: blocker-peeling P4-P9 generated repairs and counterfactuals,
  coordinated with Q3 source-derived board electrical configuration.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
- P0-P3 direct Claude review: pre-review attempts timed out; first pass-end
  review rejected missing header dependency and unsupported-ASan visibility;
  both were corrected. The second complete-file review returned PASS.
