# Current status

- Branch: `simulator`.
- Last completed pass: P8 closed-loop LED electrical feedback and BP-T001
  selected-part color-mapping admission.
- Current baseline: 93/93 CTest cases passed against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active item: run the full generated campaigns, delta minimization, and
  candidate councils.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
- P4-P7/P9 direct Claude review: the first tool-enabled gate stalled; the exact
  staged-diff gate found a non-causal newline-count assumption and an untested
  intermediate `set_output()` direction state. Both were corrected with
  explicit CRLF and isolated direction witnesses before the 89/89 run.
- P8 admission result: direct non-interactive Claude source/spec,
  simulator-fidelity, duplicate/root-cause, and final-council reviews were
  preserved in `BLOCKER_PEELING_THEORY_LOG.md`; the final council accepted
  BP-T001 after the official selected-part pinout resolved the color identity.
- P8 implementation gate: the exact staged diff received a direct
  non-interactive Claude PASS with six hardening observations. Ambiguous mapping
  names, rail-endpoint validation, local shadowing, and the open-anode
  counterfactual were improved; the focused follow-up review returned PASS and
  the final full run remained 93/93.
